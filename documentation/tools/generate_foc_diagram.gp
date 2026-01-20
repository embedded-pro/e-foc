# FOC Block Diagram - Improved Layout
set terminal svg size 1000,600 fixed enhanced font 'Arial,12'
set output 'documentation/theory/images/foc_block_diagram.svg'

unset border
unset tics
unset key
set size ratio -1
set xrange [0:20]
set yrange [0:12]

# Styles
set style line 1 lc rgb 'black' lw 2
set style arrow 1 head filled size screen 0.02,15,45 ls 1

# --- Macros for Boxes ---
# Rect (x1, y1, x2, y2)
# PI Regulators (Top-Middle)
set object 1 rect from 6,8 to 9,10 fc rgb "white" lw 2
set label "Iq\nPI Regulator" at 7.5,9 center

set object 2 rect from 6,5 to 9,7 fc rgb "white" lw 2
set label "Id\nPI Regulator" at 7.5,6 center

# Inverse Park (Top-Right)
set object 3 rect from 11,6.5 to 13,8.5 fc rgb "white" lw 2
set label "Inverse\nPark" at 12,7.5 center

# SVPWM (Right)
set object 4 rect from 14,6.5 to 16,8.5 fc rgb "white" lw 2
set label "SVPWM" at 15,7.5 center

# Inverter/Motor (Far Right/Bottom)
set object 5 rect from 17,5 to 19,9 fc rgb "white" lw 2
set label "MOSFET\nBridges" at 18,7 center

set object 6 circle at 18,2 size 1.5 fc rgb "white" lw 2
set label "Motor" at 18,0.2 center

# Sensor (Bottom Right)
set object 7 circle at 15,2 size 1 fc rgb "white" lw 2
set label "Sensor" at 15,0.7 center

# Angle Capture (Bottom Middle)
set object 8 rect from 11,1 to 13,3 fc rgb "white" lw 2
set label "Angle\nCapture" at 12,2 center

# Clarke (Bottom-Middle-Right)
set object 9 rect from 14,3.5 to 16,5.5 fc rgb "white" lw 2
set label "Clarke" at 15,4.5 center

# Park (Middle)
set object 10 rect from 11,3.5 to 13,5.5 fc rgb "white" lw 2
set label "Park" at 12,4.5 center

# --- Summing Junctions (Circles) - Left ---
set object 11 circle at 4,9 size 0.3 fc rgb "white" lw 2
set object 12 circle at 4,6 size 0.3 fc rgb "white" lw 2

# Signs for junctions
set label "+" at 3.5,9.3
set label "-" at 4.2,8.5
set label "+" at 3.5,6.3
set label "-" at 4.2,5.5

# --- Connections ---

# 1. References to PIs
# Iq Ref
set arrow from 1,9 to 3.7,9 as 1
set label "Desired Torque\nCurrent (Iq*)" at 2.5,9.5 center

# Id Ref
set arrow from 1,6 to 3.7,6 as 1
set label "Desired Flux\nCurrent (Id*)" at 2.5,6.5 center

# Junctions to PIs
set arrow from 4.3,9 to 6,9 as 1
set arrow from 4.3,6 to 6,6 as 1

# 2. PIs to Inv Park (Vq, Vd)
set arrow from 9,9 to 11,8 as 1
set label "Vq" at 10,8.8 center
set arrow from 9,6 to 11,7 as 1
set label "Vd" at 10,6.2 center

# 3. Inv Park to SVPWM (Valpha, Vbeta)
set arrow from 13,7.8 to 14,7.8 as 1
set label "V{/Symbol a}" at 13.5,8.1 center
set arrow from 13,7.2 to 14,7.2 as 1
set label "V{/Symbol b}" at 13.5,6.9 center

# 4. SVPWM to Inverter
set arrow from 16,8 to 17,8 as 1
set arrow from 16,7.5 to 17,7.5 as 1
set arrow from 16,7 to 17,7 as 1

# 5. Inverter to Motor (3 Phase)
set arrow from 18,5 to 18,3.5 as 1

# 6. Current Feedback (Motor -> Clarke)
set arrow from 18,4.5 to 19.5,4.5 nohead ls 1
set arrow from 19.5,4.5 to 19.5,4 nohead ls 1
set arrow from 19.5,4 to 16,4 as 1
set label "Ia, Ib" at 17,4.3 center
# (Simpler visual representation: pulling lines from motor line)

# 7. Clarke to Park (Ialpha, Ibeta)
set arrow from 14,4.8 to 13,4.8 as 1
set label "I{/Symbol a}" at 13.5,5.1 center
set arrow from 14,4.2 to 13,4.2 as 1
set label "I{/Symbol b}" at 13.5,3.9 center

# 8. Park to Summing Junctions (Feedback)
# Park -> iq (Feedback to top junction)
set arrow from 11,4.8 to 10,4.8 nohead ls 1
set arrow from 10,4.8 to 10,7.5 nohead ls 1
set arrow from 10,7.5 to 4,7.5 nohead ls 1
set arrow from 4,7.5 to 4,8.7 as 1
set label "Iq" at 5,7.8 center

# Park -> id (Feedback to bottom junction)
set arrow from 11,4.2 to 10.5,4.2 nohead ls 1
set arrow from 10.5,4.2 to 10.5,5 nohead ls 1
set arrow from 10.5,5 to 4,5 nohead ls 1
set arrow from 4,5 to 4,5.7 as 1
set label "Id" at 5,4.8 center

# 9. Angle Feedback (Theta)
# Sensor -> Capture
set arrow from 14,2 to 13,2 as 1
# Capture -> Park & Inv Park
set arrow from 11,2 to 10.5,2 nohead ls 1
set arrow from 10.5,2 to 10.5,3.2 nohead ls 1
set arrow from 10.5,3.2 to 12.5,3.2 nohead ls 1
set arrow from 12.5,3.2 to 12,3.5 as 1 # To Park
set arrow from 10.5,3.2 to 10.5,6 nohead ls 1
set arrow from 10.5,6 to 12,6 nohead ls 1
set arrow from 12,6 to 12,6.5 as 1 # To Inv Park
set label "{/Symbol q}" at 10.3, 2.5 center

# Mechanical linkage
set arrow from 18,2 to 16,2 nohead ls 1

plot NaN notitle
