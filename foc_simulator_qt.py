import sys
import numpy as np
from PyQt5.QtWidgets import (QApplication, QMainWindow, QWidget, QVBoxLayout, 
                             QHBoxLayout, QPushButton, QLabel, QSlider, QGroupBox,
                             QGridLayout, QSizePolicy)
from PyQt5.QtCore import QTimer, Qt
from PyQt5.QtGui import QFont
import pyqtgraph as pg

class FOCSimulator(QMainWindow):
    def __init__(self):
        super().__init__()
        self.setWindowTitle("FOC Motor Controller Simulator")
        self.setGeometry(100, 100, 1400, 800)
        
        # Simulation state
        self.running = False
        self.aligned = False
        self.elec_identified = False
        self.mech_identified = False
        
        self.speed = 0.0
        self.speed_setpoint = 0.0
        self.position = 0.0
        self.current_a = 0.0
        self.current_b = 0.0
        self.current_c = 0.0
        self.time = 0.0
        
        # Parameters
        self.R = None
        self.L = None
        self.B = None
        self.J = None
        self.Kp_I = None
        self.Ki_I = None
        self.Kp_S = None
        self.Ki_S = None
        self.Kd_S = None
        
        # Data buffers
        self.time_data = []
        self.current_a_data = []
        self.current_b_data = []
        self.current_c_data = []
        self.position_data = []
        self.speed_data = []
        self.max_points = 500
        
        # Setup UI
        self.init_ui()
        
        # Setup timer for simulation
        self.timer = QTimer()
        self.timer.timeout.connect(self.update_simulation)
        
    def init_ui(self):
        central_widget = QWidget()
        self.setCentralWidget(central_widget)
        main_layout = QHBoxLayout(central_widget)
        
        # Left panel - Controls
        left_panel = self.create_left_panel()
        main_layout.addWidget(left_panel, stretch=1)
        
        # Middle panel - Parameters
        middle_panel = self.create_middle_panel()
        main_layout.addWidget(middle_panel, stretch=2)
        
        # Right panel - Charts
        right_panel = self.create_right_panel()
        main_layout.addWidget(right_panel, stretch=3)
        
    def create_left_panel(self):
        panel = QWidget()
        layout = QVBoxLayout(panel)
        
        # Calibration group
        calib_group = QGroupBox("Calibration")
        calib_layout = QVBoxLayout()
        
        self.align_btn = QPushButton("🧭 Align Motor")
        self.align_btn.clicked.connect(self.align_motor)
        calib_layout.addWidget(self.align_btn)
        
        self.ident_elec_btn = QPushButton("⚡ Identify Electrical")
        self.ident_elec_btn.clicked.connect(self.identify_electrical)
        calib_layout.addWidget(self.ident_elec_btn)
        
        self.ident_mech_btn = QPushButton("⚙️ Identify Mechanical")
        self.ident_mech_btn.clicked.connect(self.identify_mechanical)
        calib_layout.addWidget(self.ident_mech_btn)
        
        calib_group.setLayout(calib_layout)
        layout.addWidget(calib_group)
        
        # Control group
        control_group = QGroupBox("Motor Control")
        control_layout = QVBoxLayout()
        
        self.start_btn = QPushButton("▶️ Start")
        self.start_btn.clicked.connect(self.start_simulation)
        control_layout.addWidget(self.start_btn)
        
        self.stop_btn = QPushButton("⏹️ Stop")
        self.stop_btn.clicked.connect(self.stop_simulation)
        self.stop_btn.setEnabled(False)
        control_layout.addWidget(self.stop_btn)
        
        # Speed control
        speed_label = QLabel("Speed Setpoint:")
        control_layout.addWidget(speed_label)
        
        self.speed_value_label = QLabel("0 RPM")
        self.speed_value_label.setAlignment(Qt.AlignCenter)
        font = QFont()
        font.setBold(True)
        self.speed_value_label.setFont(font)
        control_layout.addWidget(self.speed_value_label)
        
        self.speed_slider = QSlider(Qt.Horizontal)
        self.speed_slider.setMinimum(-3000)
        self.speed_slider.setMaximum(3000)
        self.speed_slider.setValue(0)
        self.speed_slider.setTickPosition(QSlider.TicksBelow)
        self.speed_slider.setTickInterval(500)
        self.speed_slider.valueChanged.connect(self.update_speed_setpoint)
        control_layout.addWidget(self.speed_slider)
        
        control_group.setLayout(control_layout)
        layout.addWidget(control_group)
        
        # Status group
        status_group = QGroupBox("Status")
        status_layout = QVBoxLayout()
        
        self.status_label = QLabel("Idle")
        self.status_label.setAlignment(Qt.AlignCenter)
        status_layout.addWidget(self.status_label)
        
        status_group.setLayout(status_layout)
        layout.addWidget(status_group)
        
        layout.addStretch()
        return panel
        
    def create_middle_panel(self):
        panel = QWidget()
        layout = QVBoxLayout(panel)
        
        # Electrical parameters
        elec_group = QGroupBox("Electrical Parameters")
        elec_layout = QGridLayout()
        
        elec_layout.addWidget(QLabel("Resistance (R):"), 0, 0)
        self.r_label = QLabel("---")
        self.r_label.setAlignment(Qt.AlignRight)
        elec_layout.addWidget(self.r_label, 0, 1)
        
        elec_layout.addWidget(QLabel("Inductance (L):"), 1, 0)
        self.l_label = QLabel("---")
        self.l_label.setAlignment(Qt.AlignRight)
        elec_layout.addWidget(self.l_label, 1, 1)
        
        elec_group.setLayout(elec_layout)
        layout.addWidget(elec_group)
        
        # Mechanical parameters
        mech_group = QGroupBox("Mechanical Parameters")
        mech_layout = QGridLayout()
        
        mech_layout.addWidget(QLabel("Friction (B):"), 0, 0)
        self.b_label = QLabel("---")
        self.b_label.setAlignment(Qt.AlignRight)
        mech_layout.addWidget(self.b_label, 0, 1)
        
        mech_layout.addWidget(QLabel("Inertia (J):"), 1, 0)
        self.j_label = QLabel("---")
        self.j_label.setAlignment(Qt.AlignRight)
        mech_layout.addWidget(self.j_label, 1, 1)
        
        mech_group.setLayout(mech_layout)
        layout.addWidget(mech_group)
        
        # Current controller gains
        curr_pid_group = QGroupBox("Current Controller (PI)")
        curr_pid_layout = QGridLayout()
        
        curr_pid_layout.addWidget(QLabel("Kp:"), 0, 0)
        self.kp_i_label = QLabel("---")
        self.kp_i_label.setAlignment(Qt.AlignRight)
        curr_pid_layout.addWidget(self.kp_i_label, 0, 1)
        
        curr_pid_layout.addWidget(QLabel("Ki:"), 1, 0)
        self.ki_i_label = QLabel("---")
        self.ki_i_label.setAlignment(Qt.AlignRight)
        curr_pid_layout.addWidget(self.ki_i_label, 1, 1)
        
        curr_pid_group.setLayout(curr_pid_layout)
        layout.addWidget(curr_pid_group)
        
        # Speed controller gains
        speed_pid_group = QGroupBox("Speed Controller (PID)")
        speed_pid_layout = QGridLayout()
        
        speed_pid_layout.addWidget(QLabel("Kp:"), 0, 0)
        self.kp_s_label = QLabel("---")
        self.kp_s_label.setAlignment(Qt.AlignRight)
        speed_pid_layout.addWidget(self.kp_s_label, 0, 1)
        
        speed_pid_layout.addWidget(QLabel("Ki:"), 1, 0)
        self.ki_s_label = QLabel("---")
        self.ki_s_label.setAlignment(Qt.AlignRight)
        speed_pid_layout.addWidget(self.ki_s_label, 1, 1)
        
        speed_pid_layout.addWidget(QLabel("Kd:"), 2, 0)
        self.kd_s_label = QLabel("---")
        self.kd_s_label.setAlignment(Qt.AlignRight)
        speed_pid_layout.addWidget(self.kd_s_label, 2, 1)
        
        speed_pid_group.setLayout(speed_pid_layout)
        layout.addWidget(speed_pid_group)
        
        layout.addStretch()
        return panel
        
    def create_right_panel(self):
        panel = QWidget()
        layout = QVBoxLayout(panel)
        
        # Current chart
        current_group = QGroupBox("Phase Currents (A, B, C)")
        current_layout = QVBoxLayout()
        
        self.current_plot = pg.PlotWidget()
        self.current_plot.setLabel('left', 'Current', units='A')
        self.current_plot.setLabel('bottom', 'Time', units='s')
        self.current_plot.addLegend()
        self.current_plot.showGrid(x=True, y=True)
        
        self.curve_a = self.current_plot.plot(pen=pg.mkPen('r', width=2), name='Phase A')
        self.curve_b = self.current_plot.plot(pen=pg.mkPen('g', width=2), name='Phase B')
        self.curve_c = self.current_plot.plot(pen=pg.mkPen('b', width=2), name='Phase C')
        
        current_layout.addWidget(self.current_plot)
        current_group.setLayout(current_layout)
        layout.addWidget(current_group)
        
        # Position/Speed chart
        pos_group = QGroupBox("Position & Speed")
        pos_layout = QVBoxLayout()
        
        self.position_plot = pg.PlotWidget()
        self.position_plot.setLabel('left', 'Position', units='rad')
        self.position_plot.setLabel('bottom', 'Time', units='s')
        self.position_plot.addLegend()
        self.position_plot.showGrid(x=True, y=True)
        
        # Create ViewBox for second Y axis (speed)
        self.speed_viewbox = pg.ViewBox()
        self.position_plot.scene().addItem(self.speed_viewbox)
        self.position_plot.getAxis('right').linkToView(self.speed_viewbox)
        self.speed_viewbox.setXLink(self.position_plot)
        self.position_plot.showAxis('right')
        self.position_plot.getAxis('right').setLabel('Speed', units='RPM')
        
        self.curve_position = pg.PlotDataItem(pen=pg.mkPen('m', width=2), name='Position')
        self.position_plot.addItem(self.curve_position)
        
        self.curve_speed = pg.PlotDataItem(pen=pg.mkPen('c', width=2), name='Speed')
        self.speed_viewbox.addItem(self.curve_speed)
        
        # Update views when resized
        def update_views():
            self.speed_viewbox.setGeometry(self.position_plot.getViewBox().sceneBoundingRect())
            self.speed_viewbox.linkedViewChanged(self.position_plot.getViewBox(), self.speed_viewbox.XAxis)
        
        update_views()
        self.position_plot.getViewBox().sigResized.connect(update_views)
        
        pos_layout.addWidget(self.position_plot)
        pos_group.setLayout(pos_layout)
        layout.addWidget(pos_group)
        
        return panel
        
    def update_speed_setpoint(self, value):
        self.speed_setpoint = value
        self.speed_value_label.setText(f"{value} RPM")
        
    def align_motor(self):
        self.align_btn.setEnabled(False)
        self.status_label.setText("Aligning motor...")
        QApplication.processEvents()
        
        QTimer.singleShot(1000, self.finish_alignment)
        
    def finish_alignment(self):
        self.aligned = True
        self.position = 0.0
        self.status_label.setText("Motor aligned")
        self.align_btn.setEnabled(True)
        
    def identify_electrical(self):
        if not self.aligned:
            self.status_label.setText("Please align motor first!")
            return
            
        self.ident_elec_btn.setEnabled(False)
        self.status_label.setText("Identifying electrical parameters...")
        QApplication.processEvents()
        
        QTimer.singleShot(1500, self.finish_elec_ident)
        
    def finish_elec_ident(self):
        # Simulate identified parameters
        self.R = 0.5 + np.random.random() * 0.3
        self.L = 0.002 + np.random.random() * 0.001
        
        # Calculate PI gains
        self.Kp_I = self.L / 0.001
        self.Ki_I = self.R / 0.001
        
        self.r_label.setText(f"{self.R:.3f} Ω")
        self.l_label.setText(f"{self.L:.4f} H")
        self.kp_i_label.setText(f"{self.Kp_I:.2f}")
        self.ki_i_label.setText(f"{self.Ki_I:.2f}")
        
        self.elec_identified = True
        self.status_label.setText("Electrical parameters identified")
        self.ident_elec_btn.setEnabled(True)
        
    def identify_mechanical(self):
        if not self.elec_identified:
            self.status_label.setText("Identify electrical parameters first!")
            return
            
        self.ident_mech_btn.setEnabled(False)
        self.status_label.setText("Identifying mechanical parameters...")
        QApplication.processEvents()
        
        QTimer.singleShot(2000, self.finish_mech_ident)
        
    def finish_mech_ident(self):
        # Simulate identified parameters
        self.B = 0.0001 + np.random.random() * 0.00005
        self.J = 0.00005 + np.random.random() * 0.00003
        
        # Calculate PID gains
        self.Kp_S = self.J / 0.01
        self.Ki_S = self.B / 0.01
        self.Kd_S = 0.00001
        
        self.b_label.setText(f"{self.B:.6f} Nm·s/rad")
        self.j_label.setText(f"{self.J:.6f} kg·m²")
        self.kp_s_label.setText(f"{self.Kp_S:.4f}")
        self.ki_s_label.setText(f"{self.Ki_S:.5f}")
        self.kd_s_label.setText(f"{self.Kd_S:.6f}")
        
        self.mech_identified = True
        self.status_label.setText("Mechanical parameters identified")
        self.ident_mech_btn.setEnabled(True)
        
    def start_simulation(self):
        if not self.mech_identified:
            self.status_label.setText("Complete all identification steps first!")
            return
            
        self.running = True
        self.time = 0.0
        self.time_data.clear()
        self.current_a_data.clear()
        self.current_b_data.clear()
        self.current_c_data.clear()
        self.position_data.clear()
        self.speed_data.clear()
        
        self.start_btn.setEnabled(False)
        self.stop_btn.setEnabled(True)
        self.status_label.setText("Running")
        
        self.timer.start(10)  # 10ms update interval
        
    def stop_simulation(self):
        self.running = False
        self.timer.stop()
        self.speed = 0.0
        self.current_a = 0.0
        self.current_b = 0.0
        self.current_c = 0.0
        
        self.start_btn.setEnabled(True)
        self.stop_btn.setEnabled(False)
        self.status_label.setText("Stopped")
        
    def update_simulation(self):
        if not self.running:
            return
            
        dt = 0.01  # 10ms timestep
        self.time += dt
        
        # Simple speed control
        speed_error = self.speed_setpoint - self.speed
        acceleration = speed_error * 5
        self.speed += acceleration * dt
        
        # Update position
        self.position += (self.speed * 2 * np.pi / 60) * dt
        
        # Simulate 3-phase currents
        electrical_angle = self.position * 7  # 7 pole pairs
        current_mag = abs(self.speed) / 3000 * 5
        
        self.current_a = current_mag * np.sin(electrical_angle)
        self.current_b = current_mag * np.sin(electrical_angle - 2 * np.pi / 3)
        self.current_c = current_mag * np.sin(electrical_angle + 2 * np.pi / 3)
        
        # Store data
        self.time_data.append(self.time)
        self.current_a_data.append(self.current_a)
        self.current_b_data.append(self.current_b)
        self.current_c_data.append(self.current_c)
        self.position_data.append(self.position % (2 * np.pi))
        self.speed_data.append(self.speed)
        
        # Keep only recent data
        if len(self.time_data) > self.max_points:
            self.time_data.pop(0)
            self.current_a_data.pop(0)
            self.current_b_data.pop(0)
            self.current_c_data.pop(0)
            self.position_data.pop(0)
            self.speed_data.pop(0)
        
        # Update plots
        self.curve_a.setData(self.time_data, self.current_a_data)
        self.curve_b.setData(self.time_data, self.current_b_data)
        self.curve_c.setData(self.time_data, self.current_c_data)
        
        self.curve_position.setData(self.time_data, self.position_data)
        self.curve_speed.setData(self.time_data, self.speed_data)

def main():
    app = QApplication(sys.argv)
    window = FOCSimulator()
    window.show()
    sys.exit(app.exec_())

if __name__ == '__main__':
    main()
