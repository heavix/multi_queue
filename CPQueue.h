// Copyright 2020 Mykhailo Velygodskyi.
// All Rights Reserved.

#pragma once
#ifndef __CPQueue_H__
#define __CPQueue_H__

#include <queue>
#include <mutex>
#include <condition_variable>
#include <cassert>

namespace 
{
    const size_t MAX_CAPACITY = 1000;
}

namespace MultyQueueProcessor
{
    /**
    \brief The CMultiQueueProcessor is a template class which allows to create and process multiple queues.
    Each queue should have unique id. All queues could be filled in a separate threads from any numbers of producers,
    but each queue is able to work with only one consumer. All the queues are processed in one separate internal thread.
    */
    template<typename T>
    class IConsumer
    {
    public:
        virtual ~IConsumer() {}
        virtual void Consume(const T &value) = 0;
    };

    
    /// Defines how the queue should work when it is full
    enum class EFullMode : int
    {
        SKIP_LAST,  /// Skip element if the queue is full
        DROP_FIRST, /// Pop first element from queue and push new one in the end
        WAIT        /// Wait till queue will be available to get new element
    };

    /**
        \brief Internal template which is is thread safe wrapper for the queue container.
         It allows to associate certain consumer to the internal queue.
    */
    template<typename T>
    class CPQueue
    {
    public:

        /**
            \brief Internal interface to notify that the queue has received new element.
        */
        class ICPQNotifier
        {
        public:
            ICPQNotifier() {}
            virtual ~ICPQNotifier() {}
            virtual void Notify() = 0;
        };

    public:
        /**
             Constructor of the queue
             \param [in] max_size - Max number of elements in queue.
             \param [in] fm - value from EFullMode enum. It defines how the queue should work when it is full.
             \param [in] skip_no_cons - boolean flag which says if the queue needs to skip elements in case of no consumer.
             \param [in] notifier - pointer to object who need to know that the queue has received new element, it should be inherited from ICPNotifier interface. 
        */
        CPQueue(size_t max_size = MAX_CAPACITY,
            EFullMode fm = EFullMode::SKIP_LAST,
            bool skip_no_cons = true,
            ICPQNotifier * notifier = nullptr) : maxSize(max_size),
            full_mode(fm),
            skip_if_no_consumer(skip_no_cons),
            notifier(notifier) {}

        ~CPQueue() {}

        /**
            It sets certain consumer to process the queue.
            \param [in] cons - pointer to consumer which inheritaed from IConsumer interface.
        */
        void SetConsumer(IConsumer<T>* cons)
        {
            std::lock_guard<std::mutex> loc(consumer_mtx);
            consumer = cons;
        }


        /**
            It push the new element to queue. Thread safe operation.
            \param [in] value - element which should be placed to the queue.
        */
        void Push(const T& value)
        {
            if (skip_if_no_consumer)
            {
                std::lock_guard<std::mutex> loc(consumer_mtx);
                if (consumer == nullptr)
                    return;
            }

            std::unique_lock<std::mutex> loc(mtx);
            const bool is_full = cpq.size() == maxSize;

            if (is_full)
            {
                if (full_mode == EFullMode::SKIP_LAST)
                {
                    return;
                }
                else if (full_mode == EFullMode::DROP_FIRST)
                {
                    cpq.pop();
                }
                else if (full_mode == EFullMode::WAIT)
                {
                    cv.wait(loc, [this]() { return cpq.size() < maxSize; });
                }
                else
                    assert(false);
            }

            cpq.push(value);
            loc.unlock();

            if (notifier)
                notifier->Notify();
        }

        /**
            Pop the element from the queue and pass it to consumer.
            \return true if element has been passed to consumer ot false in other way. 
        */
        bool Consume()
        {
            std::lock_guard<std::mutex> consumer_loc(consumer_mtx);
            if (consumer == nullptr)
                return false;

            std::unique_lock<std::mutex> q_loc(mtx);
            const bool is_empty = cpq.size() == 0;
            if (is_empty)
            {
                return false;
            }
            else
            {
                consumer->Consume(cpq.front());
                cpq.pop();
            }

            if (full_mode == EFullMode::WAIT)
            {
                cv.notify_all();
            }

            q_loc.unlock();

            return true;
        }

        /**
            It push the new element to queue. Thread safe operation.
            \return number of elements in queue. 
        */
        size_t size() const
        {
            std::lock_guard<std::mutex> loc(mtx);
            return cpq.size();
        }

        /**
            It cleares the queue.
        */
        void Clear()
        {
            std::unique_lock<std::mutex> loc(mtx);
            cpq = {};
            loc.unlock();
            if (full_mode == EFullMode::WAIT)
            {
                cv.notify_all();
            }
        }

    private:
        std::condition_variable cv;
        mutable std::mutex mtx;
        std::queue<T> cpq;

        size_t maxSize;
        ICPQNotifier* notifier;
        EFullMode full_mode;
        bool skip_if_no_consumer;

        std::mutex consumer_mtx;
        IConsumer<T>* consumer = nullptr;
    };

} // end namespace MultyQueueProcessor

#endif
