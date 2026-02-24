#include "source/simulator/view/Plot.hpp"
#include <iomanip>
#include <sstream>

namespace simulator
{
    Plot::Plot(ThreePhaseMotorModel& model, const std::string& title, const std::string& filename, const std::filesystem::path& outputDirectory, std::chrono::microseconds timeStep, std::chrono::milliseconds simulationTime)
        : ThreePhaseMotorModelObserver(model)
        , title(title)
        , filename(filename)
        , outputDirectory(outputDirectory)
        , timeStep(timeStep)
    {
        const auto steps = static_cast<std::size_t>(std::chrono::duration_cast<std::chrono::duration<float>>(simulationTime).count() / std::chrono::duration_cast<std::chrono::duration<float>>(timeStep).count());

        time.reserve(steps);
        i_a.reserve(steps);
        i_b.reserve(steps);
        i_c.reserve(steps);
        theta.reserve(steps);
    };

    void Plot::PhaseCurrentsWithMechanicalAngle(foc::PhaseCurrents currentPhases, foc::Radians theta)
    {
        auto [i_a, i_b, i_c] = currentPhases;
        auto currentTime = static_cast<float>(time.size()) * std::chrono::duration_cast<std::chrono::duration<float>>(timeStep).count();

        time.push_back(currentTime);
        this->i_a.push_back(i_a.Value());
        this->i_b.push_back(i_b.Value());
        this->i_c.push_back(i_c.Value());
        this->theta.push_back(theta.Value());
    }

    void Plot::Finished()
    {
        sciplot::Plot plot1;
        plot1.xlabel("Time [s]");
        plot1.ylabel("Phase Currents [A]");
        plot1.xrange(0.0, time.back());
        plot1.yrange(*std::min_element(i_a.begin(), i_a.end()) * 1.1, *std::max_element(i_a.begin(), i_a.end()));
        plot1.legend()
            .atOutsideRight()
            .displayHorizontal()
            .displayExpandWidthBy(2);

        plot1.drawCurve(time, i_a).label("i_a").lineWidth(2).lineColor("blue");
        plot1.drawCurve(time, i_b).label("i_b").lineWidth(2).lineColor("orange");
        plot1.drawCurve(time, i_c).label("i_c").lineWidth(2).lineColor("green");

        sciplot::Plot plot2;
        plot2.xlabel("Time [s]");
        plot2.ylabel("Electrical Angle [rad]");
        plot2.xrange(0.0, time.back());
        plot2.yrange(0.0, 2.0 * M_PI * 1.1);

        plot2.drawCurve(time, theta).label("theta").lineWidth(2).lineColor("blue");

        sciplot::Plot plot3;
        plot3.xlabel("Time [s]");
        plot3.ylabel("Phase Currents [A]");

        auto zoom_end = static_cast<float>(time.back());
        auto zoom_start = std::max(zoom_end - 0.2f, 0.0f);

        auto start_it = std::lower_bound(time.begin(), time.end(), zoom_start);
        auto start_idx = std::distance(time.begin(), start_it);

        auto i_a_zoom_min = *std::min_element(i_a.begin() + start_idx, i_a.end());
        auto i_a_zoom_max = *std::max_element(i_a.begin() + start_idx, i_a.end());
        auto i_b_zoom_min = *std::min_element(i_b.begin() + start_idx, i_b.end());
        auto i_b_zoom_max = *std::max_element(i_b.begin() + start_idx, i_b.end());
        auto i_c_zoom_min = *std::min_element(i_c.begin() + start_idx, i_c.end());
        auto i_c_zoom_max = *std::max_element(i_c.begin() + start_idx, i_c.end());

        auto zoom_current_min = std::min({ i_a_zoom_min, i_b_zoom_min, i_c_zoom_min });
        auto zoom_current_max = std::max({ i_a_zoom_max, i_b_zoom_max, i_c_zoom_max });

        plot3.xrange(zoom_start, zoom_end);
        plot3.yrange(zoom_current_min * 1.1, zoom_current_max * 1.1);
        plot3.legend()
            .atOutsideRight()
            .displayHorizontal()
            .displayExpandWidthBy(2);

        plot3.drawCurve(time, i_a).label("i_a").lineWidth(2).lineColor("blue");
        plot3.drawCurve(time, i_b).label("i_b").lineWidth(2).lineColor("orange");
        plot3.drawCurve(time, i_c).label("i_c").lineWidth(2).lineColor("green");

        sciplot::Figure fig = { { plot1 }, { plot2 }, { plot3 } };
        fig.title(title);
        fig.size(950, 1200);

        std::filesystem::create_directories(outputDirectory);

        auto now = std::chrono::system_clock::now();
        auto timeT = std::chrono::system_clock::to_time_t(now);
        std::ostringstream timestamp;
        timestamp << std::put_time(std::localtime(&timeT), "%Y-%m-%d_%H-%M-%S");

        auto outputPath = outputDirectory / (filename + "_" + timestamp.str());
        fig.save(outputPath.string() + ".png");
        fig.save(outputPath.string() + ".pdf");
    }
}
