// NowSound library by Rob Jellinghaus, https://github.com/RobJellinghaus/NowSound
// Licensed under the MIT license

#include "stdafx.h"

#include "Check.h"
#include "Histogram.h"

using namespace NowSound;

Histogram::Histogram(int capacity)
    : _capacity{ capacity },
    _min{ 0 },
    _max{ 0 },
    _total{ 0 },
    _minMaxKnown{ false },
    _valuesInInsertionOrder{}
{
    Check(capacity > 0);
}

void Histogram::Add(float value)
{
    AddImpl(value);
}

void Histogram::AddAll(const float* data, int count, bool absoluteValue)
{
    for (int i = 0; i < count; i++)
    {
        AddImpl(absoluteValue ? (float)std::abs(data[i]) : data[i]);
    }
}

void Histogram::AddImpl(float value)
{
    if (_valuesInInsertionOrder.size() == 0)
    {
        _valuesInInsertionOrder.push_back(value);
        _min = _max = _total = value;
        _minMaxKnown = true;
    }
    else
    {
        bool atCapacity = _valuesInInsertionOrder.size() == _capacity;

        if (atCapacity)
        {
            float oldValue = _valuesInInsertionOrder.front();
            _valuesInInsertionOrder.pop_front();
            _total -= oldValue;

            _minMaxKnown = _minMaxKnown && (oldValue > _min && oldValue < _max);
        }

        _valuesInInsertionOrder.push_back(value);
        _total += value;
        // Update _min or _max if applicable.  Note that this doesn't change _minMaxKnown.
        if (value < _min)
        {
            _min = value;
        }
        else if (value > _max)
        {
            _max = value;
        }
    }

    _average = _total / _valuesInInsertionOrder.size();
}

void Histogram::EnsureMinMaxKnown()
{
    if (!_minMaxKnown && _valuesInInsertionOrder.size() > 0)
    {
        _min = _valuesInInsertionOrder[0];
        _max = _valuesInInsertionOrder[0];
        for (int i = 1; i < _valuesInInsertionOrder.size(); i++)
        {
            _min = std::min<float>(_min, _valuesInInsertionOrder[i]);
            _max = std::max<float>(_max, _valuesInInsertionOrder[i]);
        }
        _minMaxKnown = true;
    }
}

float Histogram::Min()
{
    EnsureMinMaxKnown();
    return _min;
}

float Histogram::Max()
{
    EnsureMinMaxKnown();
    return _max;
}

float Histogram::Average()
{
    return _average;
}
