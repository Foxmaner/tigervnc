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

#Read info.txt into a dictionary of key -> value (as text)
info = {}
info_path = os.path.join(result_path, "info.txt")
if os.path.exists(info_path):
    with open(info_path) as f:
        for line in f:
            key, sep, value = line.strip().partition("=")
            if sep:
                info[key] = value

#Keys in info.txt that describe the run rather than the VideoDetector.
#Everything else, e.g. settings added by hand, is shown as a setting
NOT_SETTINGS = {"test", "video", "video_size", "duration", "geometry",
                "sample_interval_s", "cores", "xvnc_args", "commit",
                "samples", "cpu_avg_percent", "cpu_max_percent",
                "cpu_time_s", "rss_avg_mib", "rss_max_mib", "rss_peak_mib"}

#Writes the VideoDetector settings in a box to the right of the graph,
#if there are any in info.txt
def add_settings(ax):
    lines = [f"{key} = {value}" for key, value in info.items()
             if key not in NOT_SETTINGS]
    if not lines:
        print("No VideoDetector settings in info.txt, skipping the settings box")
        return

    ax.text(1.02, 0.5, "VideoDetector settings\n\n" + "\n".join(lines),
            transform=ax.transAxes, ha="left", va="center",
            family="monospace", fontsize=8,
            bbox=dict(boxstyle="round", facecolor="white", edgecolor="gray"))

#Load in CSV
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

#To the right of the memory graph, so it ends up right of both graphs
add_settings(ax_mem)

    #Export
plt.savefig(os.path.join(graphs_dir, "cpu_memory.pdf"), bbox_inches="tight")
plt.close()