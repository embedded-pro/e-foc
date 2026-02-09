#pragma once

#include <sciplot/sciplot.hpp>
#include <vector>

namespace graphics
{
    class PlotResults
    {
    public:
        struct Currents
        {
            std::vector<float> i_a;
            std::vector<float> i_b;
            std::vector<float> i_c;
        };

        PlotResults(const std::string title, const std::string& filename);
        void Save(const std::vector<float>& time, const Currents& currents, const std::vector<float>& theta);

    private:
        std::string title;
        std::string filename;
    };
}
