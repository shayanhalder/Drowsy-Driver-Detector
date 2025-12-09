import matplotlib.pyplot as plt
import pandas as pd
from datetime import datetime


def generate_drowsiness_graph(csv_path='logs/drowsiness_log.csv', output_path='drowsiness_graph.png'):
    """
    Generate a time series graph of drowsiness scores over time.
    
    Args:
        csv_path: Path to the drowsiness log CSV file
        output_path: Path where the graph image will be saved
    """
    # Read the CSV file
    df = pd.read_csv(csv_path)
    
    # Convert timestamp strings to datetime objects
    df['timestamp'] = pd.to_datetime(df['timestamp'])
    
    # Create the plot
    plt.figure(figsize=(12, 6))
    plt.plot(df['timestamp'], df['score'], marker='o', linestyle='-', linewidth=2, markersize=4)
    
    # Customize the plot
    plt.xlabel('Time', fontsize=12)
    plt.ylabel('Drowsiness Score', fontsize=12)
    plt.title('Drowsiness Score Over Time', fontsize=14, fontweight='bold')
    plt.grid(True, alpha=0.3)
    plt.xticks(rotation=45)
    plt.tight_layout()
    
    # Save the plot
    plt.savefig(output_path, dpi=300, bbox_inches='tight')
    print(f"Graph saved to {output_path}")
    
    # Display the plot
    plt.show()


if __name__ == '__main__':
    generate_drowsiness_graph()





