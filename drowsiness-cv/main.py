from flask import Flask, request, jsonify
import numpy as np
import cv2
from drowsiness import detect_drowsiness
from drowsy_logging import log_drowsiness_score
import os
import glob

app = Flask(__name__)

image_counter = 1

@app.route('/detect', methods=['POST'])
def receive_data():
    if not request.content_type or 'image/jpeg' not in request.content_type:
        return jsonify({"error": "Content-Type must be image/jpeg"}), 400
    
    image_bytes = request.data
    img_array = np.frombuffer(image_bytes, np.uint8)
    img = cv2.imdecode(img_array, cv2.IMREAD_COLOR)
    
    if img is None:
        print("Failed to decode image. File may be corrupted or not a valid image.")
        return jsonify({"error": "Failed to decode image. File may be corrupted or not a valid image."}), 400

    global image_counter
    # write image to disk for debugging
    with open(f'images/debug_image_{image_counter}.jpg', 'wb') as f:
        f.write(image_bytes)
    
    image_counter += 1
    is_drowsy, score = detect_drowsiness(img)
    log_drowsiness_score(is_drowsy, score)
    
    return jsonify({"drowsy": is_drowsy, "score": score}), 200

@app.route('/', methods=['GET'])
def home():
    return jsonify({"message": "Hello, World!"}), 200

if __name__ == '__main__':    
    image_files = glob.glob('images/*')
    for file_path in image_files:
        try:
            os.remove(file_path)
        except Exception as e:
            print(f"Error removing {file_path}: {e}")
    os.remove("logs/drowsiness_log.csv") if os.path.exists("logs/drowsiness_log.csv") else None
    
    app.run(host='0.0.0.0', port=5241)

