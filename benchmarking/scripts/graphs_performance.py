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
graphs_dir = os.path.join(script_dir, "..", "graphs",args.result_folder + "_baseline")
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
performance_data = pd.read_csv(os.path.join(result_path, "performance.csv"))

#CPU performance

cpu_series = pd.Series(
    performance_data["cpu_percent"].values,
    index=pd.to_timedelta(performance_data["time"], unit="s")
)
cpu_rolling = cpu_series.rolling("1s").mean()

    #Create graph
fig, (ax_cpu, ax_mem) = plt.subplots(1, 2, figsize=(14, 5))


ax_cpu.plot(performance_data["time"], performance_data["cpu_percent"], alpha=0.3, label="Raw")
ax_cpu.plot(performance_data["time"], cpu_rolling, label="1 s rolling average")
cpu_avg = performance_data["cpu_percent"].mean()
ax_cpu.axhline(cpu_avg, color="red", linestyle="--", label=f"Average: {cpu_avg:.1f} %")
ax_cpu.legend()


ax_mem.plot(performance_data["time"], performance_data["rss_mib"], label="Raw")
ax_mem.set_ylim(0, performance_data["rss_mib"].max() * 1.5)
mem_avg = performance_data["rss_mib"].mean()
ax_mem.axhline(mem_avg, color="red", linestyle="--", label=f"Average: {mem_avg:.1f} MiB")
ax_mem.legend()

    #Add titles and labels
fig.suptitle(f"{video_name}: CPU and memory usage")

ax_cpu.set_title("CPU usage")
ax_cpu.set_xlabel("Time (s)")
ax_cpu.set_ylabel("CPU (%)")
ax_cpu.set_ylim(bottom=0)
ax_cpu.grid(True)

ax_mem.set_title("Memory usage (RSS)")
ax_mem.set_xlabel("Time (s)")
ax_mem.set_ylabel("RSS (MiB)")
ax_mem.set_ylim(bottom=0)
ax_mem.grid(True)

    #Export
plt.savefig(os.path.join(graphs_dir, "cpu_memory.pdf"), bbox_inches="tight")
plt.close()