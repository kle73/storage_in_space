/*
 * Copyright (c) 2011 The Boeing Company
 *
 * SPDX-License-Identifier: GPL-2.0-only
 *
 * Author: Drishti Oza
 */
#ifndef WBAN_INTERFERENCE_HELPER_H
#define WBAN_INTERFERENCE_HELPER_H

#include "ns3/ptr.h"
#include "ns3/simple-ref-count.h"

#include <set>

namespace ns3
{

class SpectrumValue;
class SpectrumModel;

namespace wban
{
/**
 * @ingroup wban
 *
 * @brief This class provides helper functions for Wban interference handling.
 */
class WbanInterferenceHelper : public SimpleRefCount<WbanInterferenceHelper>
{
  public:
    /**
     * Create a new interference helper for the given SpectrumModel.
     *
     * @param spectrumModel the SpectrumModel to be used
     */
    WbanInterferenceHelper(Ptr<const SpectrumModel> spectrumModel);

    ~WbanInterferenceHelper();

    /**
     * Add the given signal to the set of accumulated signals. Never add the same
     * signal more than once. The SpectrumModels of the signal and the one used
     * for instantiation of the helper have to be the same.
     *
     * @param signal the signal to be added
     * @return false, if the signal was not added because the SpectrumModel of the
     * signal does not match the one of the helper, true otherwise.
     */
    bool AddSignal(Ptr<const SpectrumValue> signal);

    /**
     * Remove the given signal to the set of accumulated signals.
     *
     * @param signal the signal to be removed
     * @return false, if the signal was not removed (because it was not added
     * before), true otherwise.
     */
    bool RemoveSignal(Ptr<const SpectrumValue> signal);

    /**
     * Remove all currently accumulated signals.
     */
    void ClearSignals();

    /**
     * Get the sum of all accumulated signals.
     *
     * @return the sum of the signals
     */
    Ptr<SpectrumValue> GetSignalPsd() const;

    /**
     * Get the SpectrumModel used by the helper.
     *
     * @return the helpers SpectrumModel
     */
    Ptr<const SpectrumModel> GetSpectrumModel() const;

  private:
    // Disable implicit copy constructors
    /**
     * @brief Copy constructor - defined and not implemented.
     */
    WbanInterferenceHelper(const WbanInterferenceHelper&);
    /**
     * @brief Copy constructor - defined and not implemented.
     * @returns
     */
    WbanInterferenceHelper& operator=(const WbanInterferenceHelper&);
    /**
     * The helpers SpectrumModel.
     */
    Ptr<const SpectrumModel> m_spectrumModel;

    /**
     * The set of accumulated signals.
     */
    std::set<Ptr<const SpectrumValue>> m_signals;

    /**
     * The precomputed sum of all accumulated signals.
     */
    mutable Ptr<SpectrumValue> m_signal;

    /**
     * Mark m_signal as dirty, whenever a signal is added or removed. m_signal has
     * to be recomputed before next use.
     */
    mutable bool m_dirty;
};
} // namespace wban
} // namespace ns3

#endif /* WBAN_INTERFERENCE_HELPER_H */
