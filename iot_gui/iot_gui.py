import sys
import threading

from PyQt5.QtWidgets import (
    QApplication, QWidget, QVBoxLayout, QPushButton,
    QLabel, QMessageBox, QHBoxLayout, QSpinBox
)
from PyQt5.QtGui import QFont
from PyQt5.QtCore import Qt
import subprocess
import paho.mqtt.publish as publish
import json

MQTT_BROKER = "localhost"
MQTT_TOPIC_CONFIG = "sensor/config"

class IoTGui(QWidget):
    def __init__(self):
        super().__init__()
        self.setWindowTitle("IoT OTA + Config")
        self.setFixedSize(300, 180)

        layout = QVBoxLayout()

        # OTA Button
        self.ota_button = QPushButton("Trigger OTA")
        self.ota_button.setFont(QFont("Arial", 12))
        self.ota_button.clicked.connect(self.start_ota)
        layout.addWidget(self.ota_button)

        # Sleep Interval Input
        sleep_layout = QHBoxLayout()
        sleep_label = QLabel("Sleep Interval (s):")
        sleep_label.setFont(QFont("Arial", 11))
        self.sleep_input = QSpinBox()
        self.sleep_input.setRange(10, 86400)
        self.sleep_input.setValue(600)
        sleep_layout.addWidget(sleep_label)
        sleep_layout.addWidget(self.sleep_input)
        layout.addLayout(sleep_layout)

        # Send Button
        self.send_button = QPushButton("Send Sleep Interval")
        self.send_button.setFont(QFont("Arial", 11))
        self.send_button.clicked.connect(self.send_sleep)
        layout.addWidget(self.send_button)

        self.setLayout(layout)

    def start_ota(self):
        def run_ota_sequence():
            try:
                # Start OTA bridge
                result = subprocess.run(["pm2", "start", "ota-bridge"], capture_output=True, text=True)
                if result.returncode != 0:
                    self.show_error("Failed to start OTA bridge:\n" + result.stderr)
                    return
                self.show_info("OTA started. Waiting for completion...")

                # Monitor logs for OTA_FINISHED
                for _ in range(60):  # 60 attempts, 2 sec interval = 2 min max
                    log_result = subprocess.run(
                        ["pm2", "logs", "ota-bridge", "--lines", "50"],
                        capture_output=True, text=True
                    )
                    if "OTA_FINISHED" in log_result.stdout:
                        self.show_info("OTA complete. Starting influx bridge...")
                        subprocess.run(["pm2", "start", "influx-bridge"])
                        return
                    time.sleep(2)

                self.show_error("OTA did not complete in time.")

            except Exception as e:
                self.show_error(str(e))

        threading.Thread(target=run_ota_sequence, daemon=True).start()

    def send_sleep(self):
        sleep_val = self.sleep_input.value()
        payload = json.dumps({"sleep": sleep_val})
        try:
            publish.single(MQTT_TOPIC_CONFIG, payload=payload, hostname=MQTT_BROKER)
            QMessageBox.information(self, "Sent", f"Sleep interval sent: {sleep_val}s")
        except Exception as e:
            QMessageBox.critical(self, "MQTT Error", str(e))

    def show_info(self, message):
        QMessageBox.information(self, "Info", message)

    def show_error(self, message):
        QMessageBox.critical(self, "Error", message)

if __name__ == "__main__":
    app = QApplication(sys.argv)
    window = IoTGui()
    window.show()
    sys.exit(app.exec_())
