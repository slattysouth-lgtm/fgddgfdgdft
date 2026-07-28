#pragma once
#include <vector>
#include <deque>
#include <cmath>
#include <algorithm>

/*  MPM — McLeod Pitch Method.
    Choisi pour sa robustesse d'octave supérieure à l'autocorrélation nue.
    C'est la pièce critique du produit (cf. SPEC §3) : tout le principe
    centre/oscillation repose sur une courbe de pitch propre.

    Version v0 : suivi glissant (rolling). Le lissage Viterbi/HMM offline
    complet viendra avec le moteur de correction.                             */
class PitchTracker
{
public:
    struct Result { float f0 = 0.0f; float clarity = 0.0f; bool voiced = false; };

    void prepare (double sampleRate, int windowSize = 1024)
    {
        sr = sampleRate;
        n  = windowSize;
        nsdf.assign (n, 0.0f);
        history.clear();
    }

    void setRange (float minHz, float maxHz) { fmin = minHz; fmax = maxHz; }

    // buf : n échantillons mono contigus
    Result process (const float* buf)
    {
        Result r;

        double energy = 0.0;
        for (int i = 0; i < n; ++i) energy += (double) buf[i] * buf[i];
        if (energy < 1.0e-4 * n) { pushHistory (0.0f); return r; } // silence

        computeNSDF (buf);

        // --- peak-picking MPM : un maximum par région positive ---
        int pos = 0;
        while (pos < n - 1 && nsdf[pos] >  0.0f) ++pos;   // fin du lobe principal
        while (pos < n - 1 && nsdf[pos] <= 0.0f) ++pos;   // 1re région négative
        if (pos == 0) pos = 1;

        int    curMax = 0;
        std::vector<int> maxima;
        while (pos < n - 1)
        {
            if (nsdf[pos] > nsdf[pos - 1] && nsdf[pos] >= nsdf[pos + 1])
            {
                if (curMax == 0 || nsdf[pos] > nsdf[curMax]) curMax = pos;
            }
            ++pos;
            if (pos < n - 1 && nsdf[pos] <= 0.0f)          // fin de région positive
            {
                if (curMax > 0) { maxima.push_back (curMax); curMax = 0; }
                while (pos < n - 1 && nsdf[pos] <= 0.0f) ++pos;
            }
        }
        if (curMax > 0) maxima.push_back (curMax);
        if (maxima.empty()) { pushHistory (0.0f); return r; }

        float highest = 0.0f;
        for (int p : maxima) highest = std::max (highest, nsdf[p]);
        const float threshold = 0.9f * highest;

        int chosen = maxima.front();
        for (int p : maxima) if (nsdf[p] >= threshold) { chosen = p; break; }

        const float tau = parabolic (chosen);
        if (tau <= 0.0f) { pushHistory (0.0f); return r; }

        float f0 = (float) (sr / tau);

        r.clarity = nsdf[chosen];
        r.f0      = f0;
        r.voiced  = (r.clarity > 0.5f && f0 >= fmin && f0 <= fmax);

        // garde anti-saut d'octave + médiane courte sur l'historique
        pushHistory (r.voiced ? f0 : 0.0f);
        if (r.voiced) r.f0 = octaveGuardedMedian (f0);

        return r;
    }

private:
    double sr = 44100.0;
    int    n  = 1024;
    float  fmin = 65.0f, fmax = 1000.0f;
    std::vector<float> nsdf;
    std::deque<float>  history;               // f0 récents (0 = non-voisé)

    void computeNSDF (const float* x)
    {
        for (int tau = 0; tau < n; ++tau)
        {
            double acf = 0.0, m = 0.0;
            for (int i = 0; i < n - tau; ++i)
            {
                acf += (double) x[i] * x[i + tau];
                m   += (double) x[i] * x[i] + (double) x[i + tau] * x[i + tau];
            }
            nsdf[(size_t) tau] = (m > 0.0) ? (float) (2.0 * acf / m) : 0.0f;
        }
    }

    float parabolic (int x) const
    {
        if (x <= 0 || x >= n - 1) return (float) x;
        const float a = nsdf[(size_t) x - 1], b = nsdf[(size_t) x], c = nsdf[(size_t) x + 1];
        const float denom = a - 2.0f * b + c;
        if (std::abs (denom) < 1.0e-9f) return (float) x;
        return (float) x + 0.5f * (a - c) / denom;
    }

    void pushHistory (float f0)
    {
        history.push_back (f0);
        while (history.size() > 5) history.pop_front();
    }

    // médiane des f0 voisés récents, en repliant les sauts d'octave
    float octaveGuardedMedian (float f0)
    {
        std::vector<float> v;
        for (float h : history) if (h > 0.0f) v.push_back (h);
        if (v.size() < 3) return f0;

        // repli d'octave vers la médiane brute
        std::vector<float> tmp = v;
        std::sort (tmp.begin(), tmp.end());
        const float med = tmp[tmp.size() / 2];

        for (float& x : v)
        {
            while (x > med * 1.5f) x *= 0.5f;
            while (x < med * 0.66f) x *= 2.0f;
        }
        std::sort (v.begin(), v.end());
        return v[v.size() / 2];
    }
};
