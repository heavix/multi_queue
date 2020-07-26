// Copyright 2020 Mykhailo Velygodskyi.
// All Rights Reserved.

#pragma once
#ifndef __CMultiQueueProcessor_H__
#define __CMultiQueueProcessor_H__

#include <unordered_map>
#include <set>
#include <thread>
#include <atomic>
#include "CPQueue.h"

namespace MultyQueueProcessor
{
    /**
        \brief The CMultiQueueProcessor is a template class which allows to create and process multiple queues.
               Each queue should have unique id. All queues could be filled in a separate threads from any numbers of producers, 
               but each queue is able to work with only one consumer. All the queues are processed in one separate internal thread.
    */
    template<typename KeyType, typename ValueType>
    class CMultiQueueProcessor : public CPQueue<ValueType>::ICPQNotifier
    {
        typedef CPQueue<ValueType> QType;
        typedef std::unique_ptr<CPQueue<ValueType>> QPtr;
        typedef CPQueue<ValueType>* RawQPtr;
    public:
        CMultiQueueProcessor() 
        {
            StartProcessing();
        }

        ~CMultiQueueProcessor()
        {
            if (running)
            {
                StopProcessing();
            }
            th.join();
        }

        /**
            It starts internal thread to process the queues.
        */
        void StartProcessing() 
        {
            if (!running)
            {
                running = true;
                th = std::thread(std::bind(&CMultiQueueProcessor::Process, this));
            }
        }

        /**
            It stops internal thread to process the queues.
        */
        void StopProcessing()
        {
            running = false;
            cv.notify_all();
        }

        /**
            It adds consumer to processing certain queue.
            \param [in] id - unique id of the certain queue.
            \param [in] consumer - certain consumer, derived from IConsumer interface.
        */
        void Subscribe(KeyType id, IConsumer<ValueType> * consumer)
        {
            const RawQPtr q = GetQueue(id);
            if (q)
            {
                q->SetConsumer(consumer);
            }

            std::lock_guard<std::mutex> key_lc{ keys_mtx };
            keys.insert(id);
        }

        /**
            It removes consumer from processing certain queue.
            \param [in] id - unique id of the certain queue.
        */
        void Unsubscribe(KeyType id)
        {
            const RawQPtr q = GetQueue(id);
            if (q)
            {
                q->SetConsumer(nullptr);
            }

            std::lock_guard<std::mutex> key_lc{ keys_mtx };
            keys.erase(id);
        }

        /**
            It creates certain queue with desired behaviour.
            \param [in] id - unique id for the queue to create.
            \param [in] fm - value from EFullMode enum. It defines how the queue should work when it is full.
            \param [in] skip_no_cons - boolean flag which says if the queue needs to skip elements in case of no consumer.
            \return  - true if the queue has been created or false in other way.
        */
        bool CreateQueue(KeyType id, EFullMode fm = EFullMode::SKIP_LAST, bool skip_no_cons = true )
        {
            bool result = false;
            std::lock_guard<std::mutex> lc{ queues_mtx };
            if (queues.find(id) == queues.end()) 
            {
                result = queues.emplace(id, std::make_unique<QType>(MAX_CAPACITY, fm, skip_no_cons, this)).second;
            }

            return result;
        }

        /**
            It deletes certain queue.
            \param [in] id - unique id of the certain queue.
        */
        void DeleteQueue(KeyType id) 
        {
            Unsubscribe(id);
            std::lock_guard<std::mutex> lc{ queues_mtx };
            queues.erase(id);        
        }

        /**
            It puts new element to certain queue.
            \param [in] id - unique id of the certain queue.
            \param [in] value - element which should be put in queue.
        */
        void Enqueue(KeyType id, ValueType value)
        {
            const RawQPtr q = GetQueue(id);
            if (q)
            {
                q->Push(value);
            }
        }

    protected:
        //implementation ICPQNotifier interface
        virtual void Notify() override 
        {
            data_ready_mtx.lock();
            data_ready = true;
            data_ready_mtx.unlock();

            cv.notify_all();
        }

        inline const RawQPtr GetQueue(KeyType id)
        {
            std::lock_guard<std::mutex> lc{ queues_mtx };
            auto it = queues.find(id);
            if (it != queues.end())
                return it->second.get();
            else
                return nullptr;
        }
    
        void Process()
        {
            std::mutex mtx;
            std::unique_lock<std::mutex> lc{mtx};
            while (running)
            {
                // Sleep while no consumers
                cv.wait(lc, [this]() {
                    std::lock_guard<std::mutex> key_loc{ keys_mtx };
                    return !keys.empty() || !running;
                });

                std::unique_lock<std::mutex> key_loc{ keys_mtx };

                for (KeyType key : keys) 
                {
                    data_ready_mtx.lock();
                    data_ready = false;
                    data_ready_mtx.unlock();

                    RawQPtr q = GetQueue(key);
                    assert(q);
                    q->Consume();

                    if (!data_ready) 
                    {
                        data_ready_mtx.lock();
                    
                        bool all_queues_empty = true;
                        for (KeyType key : keys) 
                        {
                            RawQPtr q = GetQueue(key);
                            assert(q);
                            if (q->size() > 0)
                            {
                                all_queues_empty = false;
                                break;
                            }
                        }

                        data_ready_mtx.unlock();
                    
                        if (all_queues_empty)
                        {
                            cv.wait(lc, [this]() { return data_ready || !running; });
                        }
                    }
                }
            }
        }

    protected:
        std::set<KeyType> keys;
        std::unordered_map<KeyType, QPtr> queues;

        bool running = false;

        std::mutex data_ready_mtx;
        bool data_ready = false;

        std::condition_variable cv;
        std::mutex keys_mtx;
        std::mutex queues_mtx;
        std::thread th;
    };
} // end namespace MultyQueueProcessor

#endif // __CMultiQueueProcessor_H__
