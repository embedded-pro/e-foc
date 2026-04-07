# =============================================================================
# e-foc Documentation Plot Generator
# Usage: gnuplot documentation/tools/generate_plots.gp
# Outputs: documentation/theory/images/*.svg
# =============================================================================

set encoding utf8

# =============================================================================
# 1. RL Circuit Step Response
# =============================================================================
set terminal svg size 640,400 fixed enhanced font 'Arial,13'
set output 'documentation/theory/images/rl_step_response.svg'

set title "RL Circuit Step Response: i(t) = (V/R)(1 − e^{−t/τ})" font 'Arial,14'
set xlabel "Time normalised to τ = L/R" font 'Arial,12'
set ylabel "Normalised current  i(t) / (V/R)" font 'Arial,12'
set xrange [0:5]
set yrange [0:1.15]
set grid xtics ytics lc rgb '#cccccc' lt 1 lw 1
set key off

# Tau marker at t=1 τ
set arrow 1 from 1,0 to 1,0.6321 nohead lc rgb '#888888' lw 1 dt 2
set arrow 2 from 0,0.6321 to 1,0.6321 nohead lc rgb '#888888' lw 1 dt 2
set label 1 "τ" at 1.05,0.05 font 'Arial,12' tc rgb '#333333'
set label 2 "63.2%" at 0.08,0.66 font 'Arial,11' tc rgb '#333333'

# Steady-state asymptote
set arrow 3 from 0,1.0 to 5,1.0 nohead lc rgb '#aaaaaa' lw 1 dt 3
set label 3 "I_{ss} = V/R" at 4.1,1.04 font 'Arial,11' tc rgb '#666666'

set style line 1 lc rgb '#0060ad' lt 1 lw 2.5

plot 1 - exp(-x) with lines ls 1 notitle


# =============================================================================
# 2. Motor Alignment — Rotor Settling Process
# =============================================================================
set terminal svg size 640,400 fixed enhanced font 'Arial,13'
set output 'documentation/theory/images/alignment_settling.svg'

reset
set title "Rotor Settling to Alignment Angle (Underdamped 2nd-Order Response)" font 'Arial,14'
set xlabel "Time (arbitrary units)" font 'Arial,12'
set ylabel "Encoder position (normalised)" font 'Arial,12'
set xrange [0:12]
set yrange [-0.3:1.5]
set grid xtics ytics lc rgb '#cccccc' lt 1 lw 1
set key top right box

# Settled threshold band
set object 1 rect from 0,0.97 to 12,1.03 fc rgb '#eef7ee' fillstyle solid 0.6 noborder
set label 1 "Settled threshold band" at 7,1.05 font 'Arial,10' tc rgb '#336633'

# Asymptote
set arrow 1 from 0,1.0 to 12,1.0 nohead lc rgb '#aaaaaa' lw 1 dt 3

# Underdamped settling function: 1 - A*exp(-zeta*t)*cos(wd*t + phi)
# zeta=0.35, wd=3
settle(t) = (t < 0.5) ? 0.0 : 1.0 - 1.05*exp(-0.35*(t-0.5))*cos(2.8*(t-0.5) - 0.15)

set style line 1 lc rgb '#dd181f' lt 1 lw 2.5
set style line 2 lc rgb '#0060ad' lt 2 lw 1.5 dt 2

plot settle(x) with lines ls 1 title "Rotor position θ_m", \
     1.0 with lines ls 2 title "Target (θ_e = 0 → aligned position)"


# =============================================================================
# 3. FOC Coordinate Systems — αβ frame and dq frame
# =============================================================================
set terminal svg size 500,500 fixed enhanced font 'Arial,13'
set output 'documentation/theory/images/foc_coordinates.svg'

reset
set title "FOC Coordinate Systems" font 'Arial,14'
set xrange [-1.4:1.4]
set yrange [-1.4:1.4]
set size square
set border 0
unset tics
unset xlabel
unset ylabel
set key off

# Unit circle
set object 1 circle at 0,0 size 1.0 fc rgb '#f5f5f5' fillstyle solid 0.5 border lc rgb '#cccccc' lw 1

# Alpha-Beta axes (stationary frame — black)
set arrow 1 from -1.3,0 to 1.3,0 lc rgb '#111111' lw 2
set arrow 2 from 0,-1.3 to 0,1.3 lc rgb '#111111' lw 2
set label 1 "α" at 1.35, 0.0 font 'Arial,13,Bold' tc rgb '#111111'
set label 2 "β" at 0.0, 1.35 font 'Arial,13,Bold' tc rgb '#111111'

# d-q axes rotated by θ_e = 40° — blue
theta_e = 40.0 * pi / 180.0
set arrow 3 from -cos(theta_e),-sin(theta_e) to cos(theta_e),sin(theta_e) lc rgb '#1a6fba' lw 2
set arrow 4 from sin(theta_e),-cos(theta_e) to -sin(theta_e),cos(theta_e) lc rgb '#1a6fba' lw 2
set label 3 "d" at cos(theta_e)+0.08, sin(theta_e)+0.05 font 'Arial,13,Bold' tc rgb '#1a6fba'
set label 4 "q" at -sin(theta_e)-0.12, cos(theta_e)+0.05 font 'Arial,13,Bold' tc rgb '#1a6fba'

# Stator current vector I_s — red, at 65°
theta_s = 65.0 * pi / 180.0
Is = 0.75
set arrow 5 from 0,0 to Is*cos(theta_s),Is*sin(theta_s) lc rgb '#cc2200' lw 2.5 filled
set label 5 "I_s" at Is*cos(theta_s)+0.07, Is*sin(theta_s)+0.05 font 'Arial,12' tc rgb '#cc2200'

# Electrical angle arc annotation
set label 6 "θ_e" at 0.28, 0.12 font 'Arial,11' tc rgb '#1a6fba'

# I_q component (projection on q-axis) — dashed green
Iq_end_x = Is*cos(theta_s - theta_e - pi/2.0)*(-sin(theta_e))
Iq_end_y = Is*cos(theta_s - theta_e - pi/2.0)*(cos(theta_e))
set arrow 6 from 0,0 to Iq_end_x,Iq_end_y lc rgb '#228833' lw 1.5 dt 2
set label 7 "I_q" at Iq_end_x-0.14, Iq_end_y+0.05 font 'Arial,11' tc rgb '#228833'

plot NaN notitle


# =============================================================================
# 4. SVM Hexagon — Voltage Vectors and Sectors
# =============================================================================
set terminal svg size 560,560 fixed enhanced font 'Arial,13'
set output 'documentation/theory/images/svm_hexagon.svg'

reset
set title "Space Vector Modulation — Voltage Vectors and Sectors" font 'Arial,14'
set xrange [-1.25:1.25]
set yrange [-1.25:1.25]
set size square
set border 0
unset tics
set key off

# Hexagon vertices (normalised, V_dc = 1)
# V1=100 at 0°, V2=110 at 60°, V3=010 at 120°, V4=011 at 180°, V5=001 at 240°, V6=101 at 300°
# magnitude = 2/3
mag = 2.0/3.0

# Sector shading (alternating light fills)
set object 1 polygon from 0,0 to mag,0 to mag*cos(pi/3),mag*sin(pi/3) to 0,0 fc rgb '#e8f0ff' fillstyle solid 0.7 noborder
set object 2 polygon from 0,0 to mag*cos(pi/3),mag*sin(pi/3) to -mag*0.5,mag*0.866 to 0,0 fc rgb '#fff0e8' fillstyle solid 0.7 noborder
set object 3 polygon from 0,0 to -mag*0.5,mag*0.866 to -mag,0 to 0,0 fc rgb '#e8ffe8' fillstyle solid 0.7 noborder
set object 4 polygon from 0,0 to -mag,0 to -mag*0.5,-mag*0.866 to 0,0 fc rgb '#ffe8e8' fillstyle solid 0.7 noborder
set object 5 polygon from 0,0 to -mag*0.5,-mag*0.866 to mag*0.5,-mag*0.866 to 0,0 fc rgb '#f0e8ff' fillstyle solid 0.7 noborder
set object 6 polygon from 0,0 to mag*0.5,-mag*0.866 to mag,0 to 0,0 fc rgb '#e8ffff' fillstyle solid 0.7 noborder

# Sector labels
set label 1  "I"   at  0.55, 0.20 font 'Arial,11,Bold' tc rgb '#334466' center
set label 2  "II"  at  0.15, 0.52 font 'Arial,11,Bold' tc rgb '#334466' center
set label 3  "III" at -0.50, 0.20 font 'Arial,11,Bold' tc rgb '#334466' center
set label 4  "IV"  at -0.50,-0.20 font 'Arial,11,Bold' tc rgb '#334466' center
set label 5  "V"   at  0.00,-0.52 font 'Arial,11,Bold' tc rgb '#334466' center
set label 6  "VI"  at  0.55,-0.20 font 'Arial,11,Bold' tc rgb '#334466' center

# Active voltage vectors
set arrow 1 from 0,0 to mag,0                     lc rgb '#0060ad' lw 2 filled
set arrow 2 from 0,0 to mag*cos(pi/3),mag*sin(pi/3)  lc rgb '#0060ad' lw 2 filled
set arrow 3 from 0,0 to -mag*0.5,mag*0.866        lc rgb '#0060ad' lw 2 filled
set arrow 4 from 0,0 to -mag,0                     lc rgb '#0060ad' lw 2 filled
set arrow 5 from 0,0 to -mag*0.5,-mag*0.866       lc rgb '#0060ad' lw 2 filled
set arrow 6 from 0,0 to mag*0.5,-mag*0.866        lc rgb '#0060ad' lw 2 filled

# Vector labels
set label 7  "V1(100)  0°"   at  mag+0.05, 0.0    font 'Arial,10' tc rgb '#0060ad'
set label 8  "V2(110) 60°"   at  mag*cos(pi/3)+0.05, mag*sin(pi/3)+0.04  font 'Arial,10' tc rgb '#0060ad'
set label 9  "V3(010) 120°"  at -mag*0.5-0.35, mag*0.866+0.04 font 'Arial,10' tc rgb '#0060ad'
set label 10 "V4(011) 180°"  at -mag-0.05, 0.04   font 'Arial,10' tc rgb '#0060ad' right
set label 11 "V5(001) 240°"  at -mag*0.5-0.35,-mag*0.866-0.04 font 'Arial,10' tc rgb '#0060ad'
set label 12 "V6(101) 300°"  at  mag*0.5+0.05,-mag*0.866-0.04 font 'Arial,10' tc rgb '#0060ad'

# Hexagon outline
set arrow 7  from mag,0 to mag*cos(pi/3),mag*sin(pi/3) nohead lc rgb '#999999' lw 1
set arrow 8  from mag*cos(pi/3),mag*sin(pi/3) to -mag*0.5,mag*0.866 nohead lc rgb '#999999' lw 1
set arrow 9  from -mag*0.5,mag*0.866 to -mag,0 nohead lc rgb '#999999' lw 1
set arrow 10 from -mag,0 to -mag*0.5,-mag*0.866 nohead lc rgb '#999999' lw 1
set arrow 11 from -mag*0.5,-mag*0.866 to mag*0.5,-mag*0.866 nohead lc rgb '#999999' lw 1
set arrow 12 from mag*0.5,-mag*0.866 to mag,0 nohead lc rgb '#999999' lw 1

# Example reference voltage vector Vref in sector I (30°)
theta_ref = 30.0 * pi / 180.0
Vref = 0.45
set arrow 13 from 0,0 to Vref*cos(theta_ref),Vref*sin(theta_ref) lc rgb '#cc2200' lw 2.5 filled
set label 13 "V_ref" at Vref*cos(theta_ref)+0.05,Vref*sin(theta_ref)+0.05 font 'Arial,11' tc rgb '#cc2200'

# Zero vectors label
set label 14 "V0,V7 (zero)" at 0.0,-0.07 font 'Arial,10' tc rgb '#888888' center

plot NaN notitle
