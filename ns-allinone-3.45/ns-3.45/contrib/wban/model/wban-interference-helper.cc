/*
 * Copyright (c) 2011 The Boeing Company
 *
 * SPDX-License-Identifier: GPL-2.0-only
 *
 * Author: Drishti Oza
 */
#include "wban-interference-helper.h"

#include "ns3/log.h"
#include "ns3/spectrum-model.h"
#include "ns3/spectrum-value.h"

namespace ns3
{
namespace wban
{
NS_LOG_COMPONENT_DEFINE("WbanInterferenceHelper");

WbanInterferenceHelper::WbanInterferenceHelper(Ptr<const SpectrumModel> spectrumModel)
    : m_spectrumModel(spectrumModel),
      m_dirty(false)
{
    m_signal = Create<SpectrumValue>(m_spectrumModel);
}

WbanInterferenceHelper::~WbanInterferenceHelper()
{
    m_spectrumModel = nullptr;
    m_signal = nullptr;
    m_signals.clear();
}

bool
WbanInterferenceHelper::AddSignal(Ptr<const SpectrumValue> signal)
{
    NS_LOG_FUNCTION(this << signal);

    bool result = false;

    if (signal->GetSpectrumModel() == m_spectrumModel)
    {
        result = m_signals.insert(signal).second;
        if (result && !m_dirty)
        {
            *m_signal += *signal;
        }
    }
    return result;
}

bool
WbanInterferenceHelper::RemoveSignal(Ptr<const SpectrumValue> signal)
{
    NS_LOG_FUNCTION(this << signal);

    bool result = false;

    if (signal->GetSpectrumModel() == m_spectrumModel)
    {
        result = (m_signals.erase(signal) == 1);
        if (result)
        {
            m_dirty = true;
        }
    }
    return result;
}

void
WbanInterferenceHelper::ClearSignals()
{
    NS_LOG_FUNCTION(this);

    m_signals.clear();
    m_dirty = true;
}

Ptr<SpectrumValue>
WbanInterferenceHelper::GetSignalPsd() const
{
    NS_LOG_FUNCTION(this);

    if (m_dirty)
    {
        // Sum up the current interference PSD.
        std::set<Ptr<const SpectrumValue>>::const_iterator it;
        m_signal = Create<SpectrumValue>(m_spectrumModel);
        for (it = m_signals.begin(); it != m_signals.end(); ++it)
        {
            *m_signal += *(*it);
        }
        m_dirty = false;
    }

    return m_signal->Copy();
}
} // namespace wban
} // namespace ns3
