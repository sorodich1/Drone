import time
import math
import requests
from huskylib import HuskyLensLibrary

class ForceLandCameraCorrection:
    def __init__(self):
        self.camera_width = 320
        self.camera_height = 240
        self.center_x = 160
        self.center_y = 120
        
        # Простые настройки
        self.resend_interval = 0.3      # 0.3 сек между коррекциями
        self.lost_threshold = 1.0       # 1 сек без QR = сброс
        
        self.drone_server_url = "http://localhost:8080"
        
        # Состояние
        self.landing_sent = False
        self.last_correction_time = 0
        
        print("Initializing HuskyLens...")
        self.hl = HuskyLensLibrary("I2C", "", address=0x32)
        print(f"HuskyLens: {self.hl.knock()}")
    
    def get_qr_data(self):
        try:
            count = self.hl.count()
            if count > 0:
                block = self.hl.blocks()
                if block and hasattr(block, 'x'):
                    return {
                        'x': block.x, 'y': block.y,
                        'width': block.width, 'height': block.height,
                        'found': True
                    }
        except Exception as e:
            print(f"QR error: {e}")
        
        return {'found': False}
    
    def calculate_correction(self, qr_x, qr_y, qr_width, qr_height):
        dx = qr_x - self.center_x
        dy = qr_y - self.center_y
        
        # Простой коэффициент: 100 пикселей = 1 метр
        px_to_m = 0.01
        
        # Ограничиваем коррекцию
        max_correction = 1.5
        corr_x = max(-max_correction, min(max_correction, -dx * px_to_m))
        corr_y = max(-max_correction, min(max_correction, -dy * px_to_m))
        
        return {
            'x': corr_x,
            'y': corr_y,
            'dx_px': dx,
            'dy_px': dy,
            'distance_px': math.sqrt(dx*dx + dy*dy)
        }
    
    def send_correction(self, correction, attempt_num=1):
        try:
            # Всегда отправляем in_position = true (посадка сразу!)
            data = {
                'correction': {
                    'x': correction['x'],
                    'y': correction['y'],
                    'in_position': True,  # <-- ГЛАВНОЕ: всегда true
                    'accuracy_px': 0.0
                }
            }
            
            print(f"\n🛬 LANDING [{attempt_num}]")
            print(f"   X={correction['x']:.3f}m, Y={correction['y']:.3f}m")
            print(f"   Offset: {correction['dx_px']:+}px, {correction['dy_px']:+}px")
            print(f"   Distance: {correction['distance_px']:.1f}px")
            
            response = requests.post(
                f"{self.drone_server_url}/apply-correction",
                json=data,
                timeout=1.0
            )
            
            if response.status_code == 200:
                print(f"   ✅ Landing command sent!")
                return True, response
            else:
                print(f"   ❌ Failed (Status: {response.status_code})")
                return False, response
                
        except Exception as e:
            print(f"   ❌ Error: {e}")
            return False, None
    
    def run(self):
        print("="*60)
        print("QUICK LANDING CONTROLLER")
        print("="*60)
        print("Logic:")
        print("  QR detected → immediate landing")
        print("  No stabilization, no waiting")
        print("="*60)
        
        qr_detected = False
        landing_sent = False
        last_correction_time = 0
        attempt_count = 0
        
        try:
            while True:
                current = time.time()
                qr = self.get_qr_data()
                
                if qr['found']:
                    # Сброс таймера потери
                    if hasattr(self, 'lost_start'):
                        delattr(self, 'lost_start')
                    
                    if not qr_detected:
                        print(f"\n🎯 QR DETECTED!")
                        print(f"   Position: ({qr['x']}, {qr['y']})")
                        print(f"   Size: {qr['width']}x{qr['height']}px")
                        qr_detected = True
                        landing_sent = False
                        attempt_count = 0
                    
                    # Расчет коррекции
                    correction = self.calculate_correction(
                        qr['x'], qr['y'], 
                        qr['width'], qr['height']
                    )
                    
                    # Отправляем коррекцию каждые 0.3 сек
                    if current - last_correction_time >= self.resend_interval:
                        attempt_count += 1
                        success, _ = self.send_correction(correction, attempt_count)
                        
                        if success:
                            last_correction_time = current
                            if not landing_sent:
                                print(f"\n🟢 LANDING SEQUENCE STARTED!")
                                landing_sent = True
                
                else:
                    # QR не найден
                    if qr_detected:
                        if not hasattr(self, 'lost_start'):
                            self.lost_start = current
                            print(f"\n⚠️ QR lost...")
                        
                        lost_time = current - self.lost_start
                        if lost_time >= self.lost_threshold:
                            print(f"\n🔄 RESET: QR missing for {lost_time:.1f}s")
                            qr_detected = False
                            landing_sent = False
                            last_correction_time = 0
                            attempt_count = 0
                            if hasattr(self, 'lost_start'):
                                delattr(self, 'lost_start')
                
                time.sleep(0.05)
                
        except KeyboardInterrupt:
            print("\n\n🛑 Stopped")

if __name__ == "__main__":
    print("\n" + "="*60)
    print("QUICK LANDING SYSTEM READY")
    print("="*60)
    print("Starting in 2 seconds...")
    time.sleep(2)
    
    camera = ForceLandCameraCorrection()
    camera.run()
