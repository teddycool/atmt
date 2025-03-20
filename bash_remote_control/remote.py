import keyboard

def on_key_press(key):
    print(f"You pressed: {key}")

def run_function():
    print("Function executed!")

# Set up the key listener
keyboard.on_press(on_key_press)

# Assign a specific key to trigger the function
keyboard.add_hotkey('f', run_function)

# Keep the program running
keyboard.wait('esc')
