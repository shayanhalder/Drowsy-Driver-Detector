import csv
from datetime import datetime
import os


def log_drowsiness_score(is_drowsy: bool, score: float) -> None:
    log_file = "logs/drowsiness_log.csv"
    file_exists = os.path.isfile(log_file)
    
    timestamp = datetime.now().strftime("%Y-%m-%d %H:%M:%S")
    
    with open(log_file, mode='a', newline='') as file:
        writer = csv.writer(file)
        
        if not file_exists:
            writer.writerow(["timestamp", "score", "is_drowsy"])
        
        writer.writerow([timestamp, score, is_drowsy])
