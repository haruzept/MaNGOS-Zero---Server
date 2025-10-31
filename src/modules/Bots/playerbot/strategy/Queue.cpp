#include "../../botpch.h"
#include "../playerbot.h"
#include "Action.h"
#include "Queue.h"

using namespace ai;


void Queue::Push(ActionBasket *action)
{
    if (!action)
    {
        return;
    }

    for (auto* basket : actions)
    {
        if (action->getAction()->getName() == basket->getAction()->getName())
        {
            if (basket->getRelevance() < action->getRelevance())
            {
                basket->setRelevance(action->getRelevance());
            }
            delete action;
            return;
        }
    }
    actions.push_back(action);
}

void Queue::Push(ActionBasket **actions)
{
    if (!actions)
    {
        return;
    }

    for (ActionBasket** action = actions; *action != nullptr; ++action)
    {
        Push(*action);
    }
}

ActionNode* Queue::Pop()
{
    float max = -1.0f;
    ActionBasket* selection = nullptr;
    for (auto* basket : actions)
    {
        if (basket->getRelevance() > max)
        {
            max = basket->getRelevance();
            selection = basket;
        }
    }
    if (selection != nullptr)
    {
        ActionNode* action = selection->getAction();
        actions.remove(selection);
        delete selection;
        return action;
    }
    return nullptr;
}

ActionBasket* Queue::Peek()
{
    float max = -1.0f;
    ActionBasket* selection = nullptr;
    for (auto* basket : actions)
    {
        if (basket->getRelevance() > max)
        {
            max = basket->getRelevance();
            selection = basket;
        }
    }
    return selection;
}

std::size_t Queue::Size() const
{
    return actions.size();
}
