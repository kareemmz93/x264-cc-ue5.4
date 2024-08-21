import socket
import av
import cv2
import numpy as np

# Set up the UDP socket
UDP_IP = "0.0.0.0"
UDP_PORT = 5000
sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
sock.bind((UDP_IP, UDP_PORT))

# Create a PyAV decoder
codec = av.CodecContext.create('h264', 'r')

# Buffer to store incoming data
data_buffer = bytearray()
END_SIGN = b'\x00\x14\x14\x14\x10\x00'  # End sign to indicate the end of a frame

while True:
    # Receive data from the UDP socket
    data, addr = sock.recvfrom(4096)  # Buffer size is 4096 bytes
    data_buffer.extend(data)

    # Check if the end sign is in the buffer
    end_sign_index = data_buffer.find(END_SIGN)
    while end_sign_index != -1:
        # Split the buffer at the end sign
        frame_data = data_buffer[:end_sign_index]
        data_buffer = data_buffer[end_sign_index + len(END_SIGN):]

        # Decode the received data
        try:
            packets = codec.parse(frame_data)
            for packet in packets:
                frames = codec.decode(packet)
                for frame in frames:
                    # Convert the frame to a numpy array
                    img = frame.to_ndarray(format='bgr24')

                    # Display the frame using OpenCV
                    cv2.imshow('Frame', img)

                    if cv2.waitKey(1) & 0xFF == ord('q'):
                        break
        except Exception as e:
            print(e)
        # Check for another end sign in the remaining buffer
        end_sign_index = data_buffer.find(END_SIGN)

# Clean up
sock.close()
cv2.destroyAllWindows()
