from pathlib import Path

import matplotlib.pyplot as plt
import numpy as np
from PIL import Image
from matplotlib.ticker import StrMethodFormatter

TEXT_WIDTH = 506.295 / 72
FONT_SIZE = 8
FIG_DIR = "../figs"
DEFAULT_DPI = 500


def metric_yticks(fct_metric, avg_ticks, p99_ticks):
    """Return the first-panel y-axis ticks for the selected FCT metric."""
    if fct_metric == "avg":
        return avg_ticks
    if fct_metric == "p99":
        return p99_ticks
    raise ValueError(f"Unsupported FCT metric: {fct_metric}")

def metric_norm_yticks(fct_metric, norm_avg_ticks, norm_p99_ticks):
    """Return the first-panel y-axis ticks for the selected FCT metric."""
    if fct_metric == "avg":
        return norm_avg_ticks
    if fct_metric == "p99":
        return norm_p99_ticks
    raise ValueError(f"Unsupported FCT metric: {fct_metric}")


def add_once(handles, labels, artist):
    """Collect one legend entry per label across all subplots."""
    label = artist.get_label()
    if label not in labels:
        handles.append(artist)
        labels.append(label)


def add_flow_size_regions(
    ax,
    xlim,
    handles,
    labels,
    *,
    cutoff=60_000_000,
    alpha=0.18,
):
    """Mark the small/large flow regions separated by the cutoff."""
    small_patch = ax.axvspan(
        xmin=0,
        xmax=cutoff,
        facecolor="lightblue",
        alpha=alpha,
        label="Small flows",
    )
    large_patch = ax.axvspan(
        xmin=cutoff,
        xmax=xlim[1],
        facecolor="lightgreen",
        alpha=alpha,
        label="Large flows",
    )
    cutoff_line = ax.axvline(
        x=cutoff,
        color="grey",
        linestyle="--",
        linewidth=0.75,
        label="Cut off\n(60 MB)",
    )

    for artist in (small_patch, large_patch, cutoff_line):
        add_once(handles, labels, artist)


def plot_variable_lines(ax, df, variable_name, variable_values, handles, labels):
    """Plot one FCT curve for each available load/seed column."""
    variable_label = variable_name.capitalize()

    for variable in variable_values:
        column_name = f"{variable_label} {variable}"
        if column_name not in df:
            continue

        (line,) = ax.plot(
            df["Size"],
            df[column_name],
            marker="o",
            markersize=1.5,
            linewidth=0.5,
            label=f"{variable_label} {round(float(variable))}%",
        )
        add_once(handles, labels, line)


def style_axis(
    ax,
    idx,
    label,
    fct_metric,
    xticks,
    xlim,
    first_yticks,
    normalized_yticks,
    *,
    font_size=FONT_SIZE,
):
    """Apply common axis scales, ticks, labels, and frame styling."""
    ax.set_xscale("log")
    ax.set_xlim(*xlim)
    ax.set_xticks(xticks)
    ax.set_title(label, fontsize=font_size)

    if idx == 0:
        ax.set_yscale("log")
        y_ticks = first_yticks
        ylabel = f"{fct_metric} FCT (ms)"
    else:
        y_ticks = normalized_yticks
        ylabel = f"Normalized {fct_metric} FCT" if idx == 1 else ""
        ax.yaxis.set_major_formatter(StrMethodFormatter("{x}"))

    ax.set_yticks(y_ticks)
    ax.set_ylim(y_ticks[0], y_ticks[-1])
    ax.set_xlabel("Flow size (bytes)", fontsize=font_size - 1)
    ax.set_ylabel(ylabel, fontsize=font_size - 1, labelpad=5)
    ax.tick_params(axis="both", labelsize=font_size - 1, length=2.5, width=0.5)
    ax.grid(True, linewidth=0.25)

    for spine in ax.spines.values():
        spine.set_linewidth(0.5)


def adjust_subplot_widths(axes, *, width_adjustment=0.015, x_offsets=None):
    """Fine-tune panel widths to leave room for the shared legend."""
    if x_offsets is None:
        x_offsets = [0, 0.005, -0.02]

    for idx, ax in enumerate(axes):
        pos = ax.get_position()
        x0 = pos.x0 + (x_offsets[idx] if idx < len(x_offsets) else 0)
        ax.set_position([x0, pos.y0, pos.width + width_adjustment, pos.height])


def trim_vertical_whitespace(path_in, path_out=None, threshold=250, pad=5, verbose=False):
    """Trim near-white rows from the top and bottom of a PNG."""
    path_in = Path(path_in)
    path_out = path_in if path_out is None else Path(path_out)

    img = Image.open(path_in).convert("RGB")
    arr = np.array(img)

    is_white_row = np.all(arr.min(axis=2) >= threshold, axis=1)
    non_white_rows = np.where(~is_white_row)[0]
    if len(non_white_rows) == 0:
        return

    top = max(0, int(non_white_rows[0]) - pad)
    bottom = min(arr.shape[0], int(non_white_rows[-1]) + pad + 1)

    cropped = img.crop((0, top, arr.shape[1], bottom))
    cropped.save(path_out)

    if verbose:
        print(f"Cropped {path_in} from height {arr.shape[0]} to {bottom - top}")


def save_and_trim(
    path,
    *,
    dpi=DEFAULT_DPI,
    bbox_inches="tight",
    threshold=250,
    pad=5,
    verbose=False,
    **savefig_kwargs,
):
    """Save the current Matplotlib figure and trim vertical whitespace."""
    path = Path(path)
    path.parent.mkdir(parents=True, exist_ok=True)
    plt.savefig(path, bbox_inches=bbox_inches, dpi=dpi, **savefig_kwargs)
    trim_vertical_whitespace(path, threshold=threshold, pad=pad, verbose=verbose)
