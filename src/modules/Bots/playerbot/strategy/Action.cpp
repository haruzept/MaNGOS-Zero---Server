#include "../../botpch.h"
#include "../playerbot.h"
#include "AiObjectContext.h"
#include "Action.h"

using namespace ai;

std::size_t NextAction::size(NextAction** actions)
{
    if (!actions)
    {
        return 0;
    }

    std::size_t size = 0;
    for (; size < 10 && actions[size]; ++size) {}
    return size;
}

NextAction** NextAction::clone(NextAction** actions)
{
    if (!actions)
    {
        return nullptr;
    }

    const auto size = NextAction::size(actions);

    NextAction** res = new NextAction*[size + 1];
    for (std::size_t i = 0; i < size; ++i)
    {
        res[i] = new NextAction(*actions[i]);
    }
    res[size] = nullptr;
    return res;
}

NextAction** NextAction::merge(NextAction** left, NextAction** right)
{
    const auto leftSize = NextAction::size(left);
    const auto rightSize = NextAction::size(right);

    NextAction** res = new NextAction*[leftSize + rightSize + 1];
    for (std::size_t i = 0; i < leftSize; ++i)
    {
        res[i] = new NextAction(*left[i]);
    }
    for (std::size_t i = 0; i < rightSize; ++i)
    {
        res[leftSize + i] = new NextAction(*right[i]);
    }
    res[leftSize + rightSize] = nullptr;

    NextAction::destroy(left);
    NextAction::destroy(right);

    return res;
}

NextAction** NextAction::array(uint8 _nil, ...)
{
    va_list vl;
    va_start(vl, _nil);

    std::size_t size = 0;
    NextAction* cur = nullptr;
    do
    {
        cur = va_arg(vl, NextAction*);
        size++;
    }
    while (cur);

    va_end(vl);

    NextAction** res = new NextAction*[size];
    va_start(vl, _nil);
    for (std::size_t i = 0; i < size; ++i)
    {
        res[i] = va_arg(vl, NextAction*);
    }
    va_end(vl);

    return res;
}

void NextAction::destroy(NextAction** actions)
{
    if (!actions)
    {
        return;
    }

    for (std::size_t i = 0; i < 10 && actions[i]; ++i)
    {
        delete actions[i];
    }
}

Value<Unit*>* Action::GetTargetValue()
{
    return context->GetValue<Unit*>(GetTargetName());
}

Unit* Action::GetTarget()
{
    return GetTargetValue()->Get();
}
