import pygame
import paho.mqtt.client as mqtt
import time
import math
import json

# Kontrollera att paho-mqtt är installerat
try:
    import paho.mqtt.client as mqtt
except ImportError:
    print("Paho MQTT saknas. Installera med: pip install paho-mqtt")
    exit()

# MQTT-inställningar
BROKER = "192.168.2.2"
PORT = 1883
TOPIC_SPEED = "sensor/speed"
TOPIC_CONTROL = "38504720f540/control"
TOPIC_DISTANCE = "38504720f540/distance"
TOPIC_STEERING = "sensor/steering"
TOPIC_SENSORS = {
    "forward": "sensor/forward",
    "backward": "sensor/backward",
    "left": "sensor/left",
    "right": "sensor/right"
}

data_received = {"forward": 130, "backward": 60, "left": 80, "right": 90}

# Initiera Pygame
pygame.init()
screen = pygame.display.set_mode((600, 600))
pygame.display.set_caption("MQTT Rover Joystick & Sensor")
font = pygame.font.Font(None, 18)

# Färger
WHITE = (255, 255, 255)
BLACK = (0, 0, 0)
BLUE = (0, 0, 255)
RED = (255, 0, 0)
GREEN = (0, 255, 0)

# Joystick-parametrar
joystick_center = (300, 300)
joystick_radius = 100
knob_pos = list(joystick_center)
joystick_active = False

# MQTT Client Setup
def on_message(client, userdata, msg):
    global data_received
    for key, topic in TOPIC_SENSORS.items():
        if msg.topic == topic:
            data_received[key] = int(msg.payload.decode())
    if msg.topic == TOPIC_DISTANCE:
#        print(json.loads(msg))
        #print(json.loads(msg.payload.decode('utf-8')))
        distances = json.loads(msg.payload.decode('utf-8'))
        data_received["forward"] = distances['front']
        data_received["left"] = distances['left']
        data_received["right"] = distances['right']
        data_received["backward"] = distances['back']

        #print(distances['front'])

client = mqtt.Client()
client.on_message = on_message
client.connect(BROKER, PORT, 60)
client.loop_start()

for topic in TOPIC_SENSORS.values():
    client.subscribe(topic)

client.subscribe(TOPIC_DISTANCE)

last_sent_time = time.time()

running = True
while running:
    screen.fill(WHITE)
        
    for event in pygame.event.get():
        if event.type == pygame.QUIT:
            running = False
        elif event.type == pygame.MOUSEBUTTONDOWN:
            x, y = event.pos
            if math.dist((x, y), joystick_center) <= joystick_radius:
                joystick_active = True
        elif event.type == pygame.MOUSEBUTTONUP:
            joystick_active = False
            knob_pos = list(joystick_center)  # Återställ joysticken till mitten
        elif event.type == pygame.MOUSEMOTION and joystick_active:
            x, y = event.pos
            dx = x - joystick_center[0]
            dy = y - joystick_center[1]
            dist = math.sqrt(dx**2 + dy**2)
            if dist > joystick_radius:
                angle = math.atan2(dy, dx)
                knob_pos[0] = joystick_center[0] + joystick_radius * math.cos(angle)
                knob_pos[1] = joystick_center[1] + joystick_radius * math.sin(angle)
            else:
                knob_pos = [x, y]
    
    # Beräkna värden
    speed = 2*int(((joystick_center[1] - knob_pos[1]) / joystick_radius) * 100)
    steering = 1.2*int(((knob_pos[0] - joystick_center[0]) / joystick_radius) * 100)
    
    # Begränsa värden
    speed = max(-100, min(100, speed))
    steering = max(-100, min(100, steering))

    payload_dict = {"motor":speed, "direction":steering}

    # Skicka data med 200ms intervall
    if time.time() - last_sent_time > 0.2:
        client.publish(TOPIC_SPEED, str(speed))
        client.publish(TOPIC_STEERING, str(steering))
        client.publish(TOPIC_CONTROL, json.dumps(payload_dict))
        last_sent_time = time.time()
    
    # Rita sensorer som rektanglar
    pygame.draw.rect(screen, GREEN, (275, 185-data_received["forward"], 50, data_received["forward"]))  # Framåt
    pygame.draw.rect(screen, GREEN, (275, 415, 50, data_received["backward"]))  # Bakåt
    pygame.draw.rect(screen, GREEN, (185-data_received["left"], 275, data_received["left"], 50))  # Vänster
    pygame.draw.rect(screen, GREEN, (415, 275, data_received["right"], 50))  # Höger
    
    # Visa aktuella värden
    speed_text = font.render(f"Speed: {speed}", True, BLACK)
    steering_text = font.render(f"Steering: {steering}", True, BLACK)
    forward_text = font.render(f"Forward: {data_received['forward']}", True, BLACK)
    backward_text = font.render(f"Reverse: {data_received['backward']}", True, BLACK)
    left_text = font.render(f"Left: {data_received['left']}", True, BLACK)
    right_text = font.render(f"Right: {data_received['right']}", True, BLACK)

    # Rita joysticken
    pygame.draw.circle(screen, BLUE, joystick_center, joystick_radius, 2)
    pygame.draw.circle(screen, RED, knob_pos, 50)

    screen.blit(speed_text, (20, 20))
    screen.blit(steering_text, (20, 40))
    screen.blit(forward_text, (270, 30))
    screen.blit(backward_text, (270, 570))
    screen.blit(left_text, (30, 300))
    screen.blit(right_text, (530, 300))
    
    pygame.display.flip()

client.loop_stop()
client.disconnect()
pygame.quit()
