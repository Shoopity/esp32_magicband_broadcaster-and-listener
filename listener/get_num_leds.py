Import("env")
def ask_num_leds():
    try:
        user_input = input("\n[ACTION REQUIRED] Enter the number of LEDs for this build (default 148): ")
        if user_input.strip() == "":
            return 148
        val = int(user_input.strip())
        if val < 5:
            print("Number too small, defaulting to 5.")
            return 5
        return val
    except Exception as e:
        print(f"Invalid input, defaulting to 148. Error: {e}")
        return 148

num_leds = ask_num_leds()
print(f"\n--- Configuring Listener Build ---")
print(f"NUM_LEDS = {num_leds}")
print(f"----------------------------------\n")

env.Append(CPPDEFINES=[("NUM_LEDS", num_leds)])
