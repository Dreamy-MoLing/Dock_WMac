#ifndef AUDIOLEVELSMOOTHING_H
#define AUDIOLEVELSMOOTHING_H

#include <QtGlobal>

namespace music {

inline qreal smoothAudioLevel(qreal previous, qreal sample)
{
    const qreal clampedPrevious = qBound(0.0, previous, 1.0);
    const qreal clampedSample = qBound(0.0, sample, 1.0);
    const qreal sampleWeight = clampedSample > clampedPrevious ? 0.65 : 0.14;
    return clampedPrevious * (1.0 - sampleWeight) + clampedSample * sampleWeight;
}

} // namespace music

#endif // AUDIOLEVELSMOOTHING_H
