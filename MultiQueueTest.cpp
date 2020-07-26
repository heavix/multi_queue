// Copyright 2020 Mykhailo Velygodskyi.
// All Rights Reserved.

#include <iostream>
#include <set>
#include <cstring>
#include "MultiQueueProcessor.h"

using namespace MultyQueueProcessor;
static const int N = 10;

template<typename KeyType, typename ValueType>
struct SGenerator 
{
    KeyType key;
    ValueType value;
    int repetition;
    int delay_msec;
};


template<typename KeyType, typename ValueType>
void Produce(CMultiQueueProcessor<KeyType, ValueType>& processor, std::vector<SGenerator<KeyType,ValueType>> generators)
{
    int total_repetition = 0;
    for ( const auto& record : generators) 
    {
        total_repetition += record.repetition;
    }

    while (total_repetition > 0)
    {
        for (auto& record : generators)
        {
            if (record.repetition > 0)
            {
                processor.Enqueue(record.key, record.value);
                if (record.delay_msec > 0)
                {
                    std::this_thread::sleep_for(std::chrono::milliseconds(record.delay_msec));
                }
                record.repetition -= 1;
                total_repetition  -= 1;
            }
        }
    }
}

class CConsumer : public IConsumer<int>
{
public:
    CConsumer(const std::string& n): name(n) {}

    void showResult() 
    {
        std::cout << "consumer " << name.c_str() << " total:" << std::endl;
        for (auto val : total_map)
        {

            std::cout << "value " << val.first << " : " << val.second << std::endl;
        }   
    }


    virtual void Consume(const int & value) override
    {
        //std::cout << "consume " << name.c_str() << " " << value << std::endl;
        auto it = total_map.find(value);
        if (it == total_map.end())
        {
            total_map.emplace(value, 1);
        }
        else 
        {
            int counter = it->second + 1;
            total_map.erase(it);
            total_map.emplace(value, counter);
        }
    }

    std::string name;
    std::unordered_map<int, int> total_map;
};


int main()
{
    CConsumer consumer_a("A"), consumer_b("B");

    SGenerator<int, int> g1{ 1,   5, 50,  1 };
    SGenerator<int, int> g2{ 2,  10, 100, 0 };
    std::vector<SGenerator<int, int>> gs1;
    gs1.push_back(g1);
    gs1.push_back(g2);
    
    {
        CMultiQueueProcessor<int, int> queue_processor;

        queue_processor.CreateQueue(g1.key);
        queue_processor.CreateQueue(g2.key);


        queue_processor.Subscribe(g1.key, &consumer_a);
        queue_processor.Subscribe(g2.key, &consumer_b);
        
 
        std::thread t1([&]() 
        { 
            Produce(queue_processor, gs1);
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        });

        t1.join();
 
    }

    consumer_a.showResult();
    consumer_b.showResult();

    std::cin.get();

    return 0;
}
