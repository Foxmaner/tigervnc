import argparse
import os
import sys

#Add command line arguments
parser = argparse.ArgumentParser("Generate a graph of performance metrics.")
parser.add_argument("result_folder", help="The folderlocation for the results")
args = parser.parse_args()

#Get the path to the results folder
script_dir = os.path.dirname(os.path.abspath(__file__))
results_dir = os.path.join(script_dir, "..", "results")
result_path = os.path.join(results_dir, args.result_folder)

#Check if the result folder exists
print(f"Checking if the result folder exists: {result_path}")

if not os.path.exists(result_path):
    print("Error: The specified result folder does not exist.")
    sys.exit(1)
else:
    print("The specified result folder exists.")