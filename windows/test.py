# This function is used to test reading user manual file 
# TODO: Combine with read_input
def show_manual():
    try:
        with open('manual.txt', 'r') as file:
            manual_text = file.read()
            print(manual_text)
    except FileNotFoundError:
        print("Error: Manual file not found.")

if __name__ == "__main__":
    while True:
        user_input = input("Enter command: ")
        
        if user_input == '--help':
            show_manual()
        else:
            print(f"Unknown command: {user_input}. Use '--help' for assistance.")
