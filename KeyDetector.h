#pragma once
#include <array>
#include <cmath>
#include <string>

/*  Détection automatique de la tonalité (SPEC §4).
    Chromagramme monophonique : chaque f0 voisé vote pour sa classe de hauteur,
    pondéré par la confiance du tracker. Corrélation aux 24 profils
    Krumhansl-Schmuckler → tonique + mode.                                     */
class KeyDetector
{
public:
    struct Key
    {
        int   root = 9;          // 0 = Do … 9 = La
        bool  minor = true;
        float confidence = 0.0f;
        std::array<int, 7> pcs { {9,11,0,2,4,5,7} }; // classes de la gamme
        std::string nameFr() const
        {
            static const char* N[12] =
                {"Do","Do#","Re","Re#","Mi","Fa","Fa#","Sol","Sol#","La","La#","Si"};
            return std::string (N[root]) + (minor ? " mineur" : " majeur");
        }
    };

    void reset() { hist.fill (0.0f); }

    void addFrame (float f0, float clarity)
    {
        if (f0 <= 0.0f || clarity <= 0.5f) return;
        const float midi = 69.0f + 12.0f * std::log2 (f0 / 440.0f);
        int pc = ((int) std::lround (midi)) % 12;
        if (pc < 0) pc += 12;
        hist[(size_t) pc] += clarity;
    }

    Key estimate() const
    {
        static const float MAJ[12] =
            {6.35f,2.23f,3.48f,2.33f,4.38f,4.09f,2.52f,5.19f,2.39f,3.66f,2.29f,2.88f};
        static const float MIN[12] =
            {6.33f,2.68f,3.52f,5.38f,2.60f,3.53f,2.54f,4.75f,3.98f,2.69f,3.34f,3.17f};

        Key best; float bestScore = -2.0f;
        for (int root = 0; root < 12; ++root)
            for (int m = 0; m < 2; ++m)
            {
                const float s = corr (m ? MIN : MAJ, root);
                if (s > bestScore) { bestScore = s; best.root = root; best.minor = (m == 1); }
            }

        static const int IV_MAJ[7] = {0,2,4,5,7,9,11};
        static const int IV_MIN[7] = {0,2,3,5,7,8,10};
        const int* iv = best.minor ? IV_MIN : IV_MAJ;

        float total = 0.0f, inScale = 0.0f;
        for (int i = 0; i < 12; ++i) total += hist[(size_t) i];
        for (int k = 0; k < 7; ++k)
        {
            const int pc = (iv[k] + best.root) % 12;
            best.pcs[(size_t) k] = pc;
            inScale += hist[(size_t) pc];
        }
        best.confidence = (total > 0.0f) ? inScale / total : 0.0f;
        return best;
    }

private:
    std::array<float, 12> hist { {} };

    float corr (const float* prof, int rot) const
    {
        float mh = 0.0f, mp = 0.0f;
        for (int i = 0; i < 12; ++i) { mh += hist[(size_t) i]; mp += prof[i]; }
        mh /= 12.0f; mp /= 12.0f;
        float num = 0.0f, d1 = 0.0f, d2 = 0.0f;
        for (int i = 0; i < 12; ++i)
        {
            const float a = hist[(size_t) ((i + rot) % 12)] - mh;
            const float b = prof[i] - mp;
            num += a * b; d1 += a * a; d2 += b * b;
        }
        return num / std::sqrt (d1 * d2 + 1.0e-12f);
    }
};
