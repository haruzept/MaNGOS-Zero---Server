#include "ActionBasket.h"
#include <cstddef>
#include <list>

#pragma once
namespace ai
{
class Queue
{
public:
    Queue() = default;
    ~Queue() = default;
    void Push(ActionBasket *action);
    void Push(ActionBasket **actions);
    ActionNode* Pop();
    ActionBasket* Peek();
    std::size_t Size() const;
private:
    std::list<ActionBasket*> actions;
};
}
