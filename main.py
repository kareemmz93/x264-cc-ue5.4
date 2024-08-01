import socket
import av
import cv2
import numpy as np

# Set up the UDP socket
UDP_IP = "127.0.0.1"
UDP_PORT = 5000

sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
sock.bind((UDP_IP, UDP_PORT))

# Create a PyAV decoder
codec = av.CodecContext.create('h264', 'r')

while True:
    # Receive data from the UDP socket
    data, addr = sock.recvfrom(65536)  # Buffer size is 65536 bytes

    # Decode the received data
    packets = codec.parse(data)
    for packet in packets:
        frames = codec.decode(packet)
        for frame in frames:
            # Convert the frame to a numpy array
            img = frame.to_ndarray(format='bgr24')

            # Display the frame using OpenCV
            cv2.imshow('Frame', img)

            if cv2.waitKey(1) & 0xFF == ord('q'):
                break

# Clean up
sock.close()
cv2.destroyAllWindows()