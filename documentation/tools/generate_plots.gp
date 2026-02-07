# Set common style
set terminal svg size 600,400 fixed enhanced font 'Arial,12'
set grid

# 1. RL Step Response
set output 'documentation/theory/images/rl_step_response.svg'
set title "RL Circuit Step Response"
set xlabel "Time (t)"
set ylabel "Current (i)"
set xrange [0:5]
set yrange [0:1.1]
set key off
set style line 1 lc rgb '#0060ad' lt 1 lw 2
set arrow from 1,0 to 1,0.632 nohead ls 2 dt 2
set arrow from 0,0.632 to 1,0.632 nohead ls 2 dt 2
set label "{/Symbol t} (L/R)" at 1.1, 0.1
set label "63.2%" at 0.1, 0.68
plot 1 - exp(-x) ls 1

# 2. Alignment Process
set output 'documentation/theory/images/alignment_settling.svg'
set title "Motor Alignment Process"
set xlabel "Time"
set ylabel "Rotor Position"
set xrange [0:10]
set yrange [-0.5:1.5]
set style line 2 lc rgb '#dd181f' lt 1 lw 2
# Simulate a damped oscillation settling to 1.0
settle(x) = (x < 1) ? 0.5 : (1.0 - 0.5*exp(-(x-1))*cos(5*(x-1)))
plot settle(x) ls 2 title "Position"

# 3. FOC Coordinate Systems (Simplified Conceptual Plot)
set output 'documentation/theory/images/foc_coordinates.svg'
set title "FOC Coordinate Systems"
set size square
set xrange [-1.2:1.2]
set yrange [-1.2:1.2]
set xlabel ""
set ylabel ""
unset grid
set border 0
unset tics
# Draw unit circle
set object 1 circle at 0,0 size 1 fc rgb "#eeeeee" lw 1
# Alpha-Beta Axes (Stationary)
set arrow 1 from -1.1,0 to 1.1,0 lc rgb "black" lw 2
set label "{/Symbol a}" at 1.15,0
set arrow 2 from 0,-1.1 to 0,1.1 lc rgb "black" lw 2
set label "{/Symbol b}" at 0,1.15
# d-q Axes (Rotating) - Rotated by 45 deg (pi/4)
set arrow 3 from -0.8,-0.8 to 0.8,0.8 lc rgb "blue" lw 2
set label "d" at 0.85,0.85
set arrow 4 from 0.8,-0.8 to -0.8,0.8 lc rgb "blue" lw 2
set label "q" at -0.85,0.85
# Vector Is
set arrow 5 from 0,0 to 0.5,0.7 lc rgb "red" lw 3
set label "I_s" at 0.52,0.72
plot NaN notitle
