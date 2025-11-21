from flask import Flask, request, jsonify
import numpy as np
import cv2
from Drowsiness_Detection import detect_drowsiness

app = Flask(__name__)

@app.route('/detect', methods=['POST'])
def receive_data():
    if not request.content_type or 'multipart/form-data' not in request.content_type:
        return jsonify({"error": "Content-Type must be multipart/form-data"}), 400
    if 'file' not in request.files:
        return jsonify({"error": "No file part in the request"}), 400

    file = request.files['file']

    if file.filename == '':
        return jsonify({"error": "No selected file"}), 400

    # Check if the file is a jpg image
    if not (file and file.filename.lower().endswith('.jpg')):
        return jsonify({"error": "File is not a .jpg image"}), 400

    image_bytes = file.read()
    img_array = np.frombuffer(image_bytes, np.uint8)
    img = cv2.imdecode(img_array, cv2.IMREAD_COLOR)
    
    if img is None:
        return jsonify({"error": "Failed to decode image. File may be corrupted or not a valid image."}), 400
    
    print(f"Received .jpg image file: {file.filename}, bytes size: {len(image_bytes)}")
    
    is_drowsy = detect_drowsiness(img)
    
    return jsonify({"drowsy": is_drowsy}), 200

@app.route('/ping', methods=['GET'])
def ping():
    return jsonify({"message": "pong"}), 200

@app.route('/', methods=['GET'])
def home():
    return jsonify({"message": "Hello, World!"}), 200

if __name__ == '__main__':
    app.run(host='0.0.0.0', port=5241)

