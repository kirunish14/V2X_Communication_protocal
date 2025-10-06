import cv2
import pickle
import cvzone
import numpy as np
import paho.mqtt.client as mqtt  # MQTT client for Python

# Replace UDP socket with MQTT client setup
MQTT_BROKER = "broker.emqx.io"
MQTT_PORT = 1883
MQTT_TOPIC = "parking/status"

client = mqtt.Client()
client.connect(MQTT_BROKER, MQTT_PORT, 60)

# Video feed
cap = cv2.VideoCapture('carPark.mp4')

with open('CarParkPos', 'rb') as f:
    posList = pickle.load(f)

width, height = 107, 48
last_space_count = -1

def checkParkingSpace(imgPro):
    global last_space_count
    spaceCounter = 0

    for idx, pos in enumerate(posList):
        x, y = pos
        imgCrop = imgPro[y:y + height, x:x + width]
        count = cv2.countNonZero(imgCrop)

        if count < 900:   # Empty
            color = (0, 255, 0)
            thickness = 5
            spaceCounter += 1
        else:             # Occupied
            color = (0, 0, 255)
            thickness = 2

        # Draw rectangle + ID
        cv2.rectangle(img, pos, (pos[0] + width, pos[1] + height), color, thickness)
        cvzone.putTextRect(img, f'ID:{idx+1}', (x, y + height - 3),
                           scale=1, thickness=2, offset=2, colorR=color)

    # Show free count on video
    cvzone.putTextRect(img, f'Free: {spaceCounter}/{len(posList)}', (100, 50),
                       scale=3, thickness=5, offset=20, colorR=(0, 200, 0))

    # Only send if value changed
    if spaceCounter != last_space_count:
        msg = f"PARKING: {spaceCounter}/{len(posList)}"
        print(msg)
        try:
            client.publish(MQTT_TOPIC, msg)
            print(f"✅ MQTT Published: {msg}")
        except Exception as e:
            print(f"⚠️ MQTT Publish failed: {e}")
        last_space_count = spaceCounter

while True:
    if cap.get(cv2.CAP_PROP_POS_FRAMES) == cap.get(cv2.CAP_PROP_FRAME_COUNT):
        cap.set(cv2.CAP_PROP_POS_FRAMES, 0)
    success, img = cap.read()
    imgGray = cv2.cvtColor(img, cv2.COLOR_BGR2GRAY)
    imgBlur = cv2.GaussianBlur(imgGray, (3, 3), 1)
    imgThreshold = cv2.adaptiveThreshold(imgBlur, 255, cv2.ADAPTIVE_THRESH_GAUSSIAN_C,
                                         cv2.THRESH_BINARY_INV, 25, 16)
    imgMedian = cv2.medianBlur(imgThreshold, 5)
    kernel = np.ones((3, 3), np.uint8)
    imgDilate = cv2.dilate(imgMedian, kernel, iterations=1)

    checkParkingSpace(imgDilate)

    cv2.imshow("Image", img)
    key = cv2.waitKey(10)
    if key == ord('q'):
        break
