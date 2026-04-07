# =============================================================================
# e-foc FOC Block Diagram Generator
# Usage: gnuplot documentation/tools/generate_foc_diagram.gp
# Output: documentation/theory/images/foc_block_diagram.svg
#
# Draws the complete FOC control loop as a block diagram using gnuplot
# boxes and arrow objects.  The diagram is accurate to the actual signal
# flow: Sensors → Clarke → Park → PI(d/q) → InvPark → SVM → Inverter → Motor.
# Outer speed/position loops are shown with lighter styling.
# =============================================================================

set terminal svg size 900,480 fixed enhanced font 'Arial,11'
set output 'documentation/theory/images/foc_block_diagram.svg'

set xrange [0:90]
set yrange [0:48]
set border 0
unset tics
unset xlabel
unset ylabel
set key off
set size ratio -1

# ---------------------------------------------------------------------------
# Helper: draw a labelled box
#  Usage:  call 'drawbox' x_centre y_centre half_width half_height "Label" color
# ---------------------------------------------------------------------------
# We use gnuplot objects (rectangles) + labels.

# ---- Color scheme ---------------------------------------------------------
# Main path:   fill #dceeff, border #3366aa
# Outer loops: fill #e8ffe8, border #338844
# Motor/Sensor: fill #fff3dd, border #997700

# ===========================================================================
# ROW 1 (y ≈ 32):  Position/Speed outer loops
# ===========================================================================
# Position PI  [4,30] to [14,34]
set object 1  rect from  4,30 to 14,34 fc rgb '#e8ffe8' fillstyle solid border lc rgb '#338844' lw 1.5
set label 1   "Position\nController" at  9,32 center font 'Arial,10'

# Speed PI  [18,30] to [28,34]
set object 2  rect from 18,30 to 28,34 fc rgb '#e8ffe8' fillstyle solid border lc rgb '#338844' lw 1.5
set label 2   "Speed\nController" at 23,32 center font 'Arial,10'

# ===========================================================================
# ROW 2 (y ≈ 20):  Inner current control loop
# ===========================================================================
# Summing junction d-axis  [32,18] circle
set object 3  circle at 35,20 size 1.5 fc rgb '#ffffff' fillstyle solid border lc rgb '#333333' lw 1.5
set label 3   "Σ" at 35,20 center font 'Arial,13'

# Summing junction q-axis  [32,14]
set object 4  circle at 35,14 size 1.5 fc rgb '#ffffff' fillstyle solid border lc rgb '#333333' lw 1.5
set label 4   "Σ" at 35,14 center font 'Arial,13'

# PI d-axis  [38,17] to [48,23]
set object 5  rect from 38,17 to 48,23 fc rgb '#dceeff' fillstyle solid border lc rgb '#3366aa' lw 1.5
set label 5   "PI\nid" at 43,20 center font 'Arial,10'

# PI q-axis  [38,11] to [48,17]
set object 6  rect from 38,11 to 48,17 fc rgb '#dceeff' fillstyle solid border lc rgb '#3366aa' lw 1.5
set label 6   "PI\niq" at 43,14 center font 'Arial,10'

# ===========================================================================
# ROW 2 (cont.): Inverse Park → SVM → Inverter
# ===========================================================================
# Inverse Park  [50,13] to [60,21]
set object 7  rect from 50,14 to 60,22 fc rgb '#dceeff' fillstyle solid border lc rgb '#3366aa' lw 1.5
set label 7   "Inv. Park\ndq→αβ" at 55,18 center font 'Arial,10'

# SVM  [62,14] to [72,22]
set object 8  rect from 62,14 to 72,22 fc rgb '#dceeff' fillstyle solid border lc rgb '#3366aa' lw 1.5
set label 8   "SVM\nCommon-mode\ninjection" at 67,18 center font 'Arial,9'

# Inverter  [74,14] to [82,22]
set object 9  rect from 74,14 to 82,22 fc rgb '#fff3dd' fillstyle solid border lc rgb '#997700' lw 1.5
set label 9   "Inverter\n3-Ph" at 78,18 center font 'Arial,10'

# Motor  [84,14] to [90,22]
set object 10 rect from 84,14 to 90,22 fc rgb '#fff3dd' fillstyle solid border lc rgb '#997700' lw 1.5
set label 10  "PMSM" at 87,18 center font 'Arial,10'

# ===========================================================================
# ROW 3 (y ≈ 6):  Sensors → Clarke → Park
# ===========================================================================
# ADC / Sensors  [1,3] to [11,9]
set object 11 rect from  1, 3 to 11, 9 fc rgb '#fff3dd' fillstyle solid border lc rgb '#997700' lw 1.5
set label 11  "ADC\nia, ib" at  6, 6 center font 'Arial,10'

# Clarke  [13,3] to [23,9]
set object 12 rect from 13, 3 to 23, 9 fc rgb '#dceeff' fillstyle solid border lc rgb '#3366aa' lw 1.5
set label 12  "Clarke\nabc→αβ" at 18, 6 center font 'Arial,10'

# Park  [25,3] to [35,9]
set object 13 rect from 25, 3 to 35, 9 fc rgb '#dceeff' fillstyle solid border lc rgb '#3366aa' lw 1.5
set label 13  "Park\nαβ→dq" at 30, 6 center font 'Arial,10'

# Encoder  [74,3] to [84,9]
set object 14 rect from 74, 3 to 84, 9 fc rgb '#fff3dd' fillstyle solid border lc rgb '#997700' lw 1.5
set label 14  "Encoder\nθ_m" at 79, 6 center font 'Arial,10'

# Angle calc  [55,3] to [65,9]
set object 15 rect from 55, 3 to 65, 9 fc rgb '#f5f5f5' fillstyle solid border lc rgb '#888888' lw 1.2
set label 15  "θ_e = p·θ_m\n−θ_offset" at 60, 6 center font 'Arial,9'

# ===========================================================================
# ARROWS — main data flow path
# ===========================================================================
# ADC → Clarke
set arrow 1  from 11,6 to 13,6 lc rgb '#2244aa' lw 1.5 filled
# Clarke → Park
set arrow 2  from 23,6 to 25,6 lc rgb '#2244aa' lw 1.5 filled
# Park → Σ_d  (id up to row2)
set arrow 3  from 30,9 to 30,15 nohead lc rgb '#2244aa' lw 1.2
set arrow 4  from 30,15 to 33.5,20 lc rgb '#2244aa' lw 1.2 filled
# Park → Σ_q
set arrow 5  from 30,15 to 33.5,14 lc rgb '#2244aa' lw 1.2 filled
# Σ_d → PI_d
set arrow 6  from 36.5,20 to 38,20 lc rgb '#2244aa' lw 1.5 filled
# Σ_q → PI_q
set arrow 7  from 36.5,14 to 38,14 lc rgb '#2244aa' lw 1.5 filled
# PI_d → InvPark
set arrow 8  from 48,20 to 50,20 lc rgb '#2244aa' lw 1.5 filled
# PI_q → InvPark
set arrow 9  from 48,14 to 50,14 lc rgb '#2244aa' lw 1.5 filled
# InvPark → SVM
set arrow 10 from 60,18 to 62,18 lc rgb '#2244aa' lw 1.5 filled
# SVM → Inverter
set arrow 11 from 72,18 to 74,18 lc rgb '#2244aa' lw 1.5 filled
# Inverter → Motor
set arrow 12 from 82,18 to 84,18 lc rgb '#2244aa' lw 1.5 filled

# Encoder → angle calc
set arrow 13 from 74,6 to 65,6 lc rgb '#997700' lw 1.5 filled
# Angle calc → Park (θ_e feedback)
set arrow 14 from 55,6 to 35,6 lc rgb '#997700' lw 1.5 filled
# Angle calc → InvPark  (θ_e up)
set arrow 15 from 60,9 to 60,14 lc rgb '#997700' lw 1.2 filled

# Motor mechanical → Encoder feedback
set arrow 16 from 87,14 to 87,9 nohead lc rgb '#997700' lw 1.2
set arrow 17 from 87,9  to 84,6  lc rgb '#997700' lw 1.2 filled

# Motor current feedback → ADC
set arrow 18 from 84,20 to 84,24 nohead lc rgb '#448844' lw 1.2 dt 2
set arrow 19 from 84,24 to  6,24 nohead lc rgb '#448844' lw 1.2 dt 2
set arrow 20 from  6,24 to  6, 9 lc rgb '#448844' lw 1.2 dt 2 filled

# ===========================================================================
# ARROWS — outer loop
# ===========================================================================
# Position ref → Position PI
set arrow 21 from  1,32 to  4,32 lc rgb '#228844' lw 1.5 filled
# Position PI → Speed PI
set arrow 22 from 14,32 to 18,32 lc rgb '#228844' lw 1.5 filled
# Speed PI → Σ_q  (iq* reference)
set arrow 23 from 28,32 to 35,32 nohead lc rgb '#228844' lw 1.2 filled
set arrow 24 from 35,32 to 35,15.5 lc rgb '#228844' lw 1.2 filled

# id* reference (zero for MTPA)
set arrow 25 from 28,22 to 33.5,21 lc rgb '#888888' lw 1.0 filled

# ===========================================================================
# LABELS — signal names
# ===========================================================================
set label 20 "i_a, i_b" at 12,6.5 font 'Arial,9' tc rgb '#2244aa'
set label 21 "iα, iβ"   at 24,6.5 font 'Arial,9' tc rgb '#2244aa'
set label 22 "i_d, i_q" at 31,11  font 'Arial,9' tc rgb '#2244aa'
set label 23 "v_d"       at 49,21  font 'Arial,9' tc rgb '#2244aa'
set label 24 "v_q"       at 49,14.5 font 'Arial,9' tc rgb '#2244aa'
set label 25 "vα, vβ"   at 61,18.8 font 'Arial,9' tc rgb '#2244aa'
set label 26 "d_A,B,C"  at 73,18.8 font 'Arial,9' tc rgb '#2244aa'
set label 27 "v_abc"     at 83,18.8 font 'Arial,9' tc rgb '#997700'
set label 28 "θ_e"       at 57,10.5 font 'Arial,9' tc rgb '#997700'
set label 29 "θ_m"       at 72,5    font 'Arial,9' tc rgb '#997700'
set label 30 "i_q*"      at 36,30   font 'Arial,9' tc rgb '#228844'
set label 31 "i_d* = 0"  at 24,22.5 font 'Arial,9' tc rgb '#888888'
set label 32 "θ* / ω*"  at -1,32   font 'Arial,9' tc rgb '#228844'
set label 33 "Current\nfeedback" at 40,25 font 'Arial,8' tc rgb '#448844'

plot NaN notitle
