import argparse
import os
import sys
import pandas as pd
import matplotlib.pyplot as plt

#Add command line arguments
parser = argparse.ArgumentParser("Generate a graph of performance metrics.")
parser.add_argument("result_folder", help="The folderlocation for the results")
args = parser.parse_args()

#Get the path to the results folder
script_dir = os.path.dirname(os.path.abspath(__file__))
results_dir = os.path.join(script_dir, "..", "results")
result_path = os.path.join(results_dir, args.result_folder)
video_name = args.result_folder.rsplit("_", 3)[0]
video_name = video_name.replace("_", " ")
video_name = video_name.capitalize()
#Get the path of the graph folder
graphs_dir = os.path.join(script_dir, "..", "graphs",args.result_folder)
os.makedirs(graphs_dir, exist_ok=True)

#Check if the result folder exists
print(f"Checking if the result folder exists: {result_path}")

if not os.path.exists(result_path):
    print("Error: The specified result folder does not exist.")
    sys.exit(1)
else:
    print("The specified result folder exists.")

#Load in CSV
#info_data = pd.read_csv(os.path.join(result_path, "info.csv"))
overlap_data = pd.read_csv(os.path.join(result_path, "overlap.csv"))
success_data = pd.read_csv(os.path.join(result_path, "success.csv"))


#Success plot graph

    #Create graph
plt.figure(figsize=(10, 6))    
plt.plot(success_data["threshold"], success_data["success_rate"], marker="o")

    #Add titles and labels
plt.title(f"{video_name}: Success plot")
plt.xlabel("Overlap threshold")
plt.ylabel("Success rate")
plt.xlim(0, 1)
plt.ylim(0, 1.05)
plt.grid(True)

    #Export
plt.savefig(os.path.join(graphs_dir, "success_plot.pdf"), bbox_inches="tight")
plt.close()

#Overlap/Precission/Recall graph

    #Create graph
plt.figure(figsize=(10, 6))    
plt.plot(overlap_data["sample"], overlap_data["overlap"], label="Overlap")
plt.plot(overlap_data["sample"], overlap_data["precision"], label="Precision")
plt.plot(overlap_data["sample"], overlap_data["recall"], label="Recall")

    #Add titles and labels
plt.title(f"{video_name}: Overlap, precision and recall")
plt.xlabel("Sample, 10 samples = 1 second")
plt.ylabel("Score")
plt.ylim(0, 1.05)
plt.grid(True)
plt.legend()

    #Export
plt.savefig(os.path.join(graphs_dir, "overlap_precision_recall.pdf"), bbox_inches="tight")
plt.close()

#Nr of cells count graph
    #Create graph
plt.figure(figsize=(10, 6))    
plt.plot(overlap_data["sample"], overlap_data["cell_rect_count"], label="Number of cells")

    #Add titles and labels
plt.title(f"{video_name}: Number of cells")
plt.xlabel("Sample, 10 samples = 1 second")
plt.ylabel("Number of rectangles")
plt.ylim(bottom=0)
plt.grid(True)

    #Export
plt.savefig(os.path.join(graphs_dir, "cell_plot.pdf"), bbox_inches="tight")
plt.close()