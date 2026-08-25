import os
from concurrent.futures import ProcessPoolExecutor
from pathlib import Path

import numpy as np
import pandas as pd


def resolve_fct_file_path(file_template, alternate_file_templates, network, variable, seed):
    """Return the primary log path, or an existing alternate naming pattern."""
    primary_path = Path(file_template.format(network=network, variable=variable, seed=seed))
    if primary_path.exists():
        return primary_path

    for alternate_template in alternate_file_templates.get(network, []):
        alternate_path = Path(alternate_template.format(variable=variable, seed=seed))
        if alternate_path.exists():
            return alternate_path

    return primary_path


def get_fct_metrics_from_file(file_path):
    """Return average and p99 FCT dictionaries keyed by flow size."""
    file_path = Path(file_path)

    if not file_path.exists():
        print(f"File not found: {file_path}. Skipping...")
        return None

    fct_by_size = {}
    with file_path.open() as file:
        for line in file:
            if not line.startswith("FCT"):
                continue

            parts = line.split()
            if len(parts) < 6:
                continue

            fct = float(parts[4])
            start = float(parts[5])
            if fct + start <= 10_000:
                fct_by_size.setdefault(int(parts[3]), []).append(fct)

    if not fct_by_size:
        return None

    avg_fct = {}
    p99_fct = {}
    for size in sorted(fct_by_size):
        values = np.asarray(fct_by_size[size], dtype=float)
        avg_fct[size] = round(float(values.mean()), 5)
        p99_fct[size] = round(float(np.percentile(values, 99)), 5)

    return avg_fct, p99_fct


def select_fct_metric(metrics, fct_metric):
    """Select one FCT metric from cached avg/p99 dictionaries."""
    if metrics is None:
        return {}
    avg_fct, p99_fct = metrics
    if fct_metric == "avg":
        return avg_fct
    if fct_metric == "p99":
        return p99_fct
    raise ValueError(f"Invalid FCT metric: {fct_metric}")


def collect_network_fct_results_for_process(
    file_template,
    alternate_file_templates,
    max_load_by_network,
    network_idx,
    network,
    fct_metric,
    variable_name,
    variable_values,
    seeds,
    verbose=False,
):
    """Collect raw FCT values for one network before cross-network normalization."""
    max_load = max_load_by_network.get(network)
    included_variables = [
        variable
        for variable in variable_values
        if max_load is None or float(variable) <= max_load
    ]
    results = {variable: {} for variable in included_variables}
    baseline_x_values = []
    baseline_x_values_seen = set()

    for variable in included_variables:
        for seed in seeds:
            file_path = resolve_fct_file_path(
                file_template,
                alternate_file_templates,
                network,
                variable,
                seed,
            )
            metrics = get_fct_metrics_from_file(file_path)
            fct_dict = select_fct_metric(metrics, fct_metric)
            if not fct_dict:
                continue

            if verbose:
                print(f"FCT data for {network} at {variable_name}={variable}, seed={seed}: {fct_dict}")

            for size, value in fct_dict.items():
                results[variable].setdefault(size, []).append(value)
                if network_idx == 0 and seed == seeds[0] and size not in baseline_x_values_seen:
                    baseline_x_values.append(size)
                    baseline_x_values_seen.add(size)

    return {
        "network_idx": network_idx,
        "network": network,
        "results": results,
        "baseline_x_values": baseline_x_values,
    }


def collect_network_all_fct_results_for_process(
    file_template,
    alternate_file_templates,
    max_load_by_network,
    network_idx,
    network,
    metrics,
    variable_name,
    variable_values,
    seeds,
    verbose=False,
):
    """Collect raw FCT values for all requested metrics in one pass per file."""
    max_load = max_load_by_network.get(network)
    included_variables = [
        variable
        for variable in variable_values
        if max_load is None or float(variable) <= max_load
    ]
    results_by_metric = {
        metric: {variable: {} for variable in included_variables}
        for metric in metrics
    }
    baseline_x_values = []
    baseline_x_values_seen = set()

    for variable in included_variables:
        for seed in seeds:
            file_path = resolve_fct_file_path(
                file_template,
                alternate_file_templates,
                network,
                variable,
                seed,
            )
            parsed_metrics = get_fct_metrics_from_file(file_path)
            if parsed_metrics is None:
                continue

            for metric in metrics:
                fct_dict = select_fct_metric(parsed_metrics, metric)
                if not fct_dict:
                    continue

                if verbose:
                    print(f"FCT data for {network} at {variable_name}={variable}, seed={seed}, metric={metric}: {fct_dict}")

                metric_results = results_by_metric[metric][variable]
                for size, value in fct_dict.items():
                    metric_results.setdefault(size, []).append(value)
                    if network_idx == 0 and seed == seeds[0] and size not in baseline_x_values_seen:
                        baseline_x_values.append(size)
                        baseline_x_values_seen.add(size)

    return {
        "network_idx": network_idx,
        "network": network,
        "results_by_metric": results_by_metric,
        "baseline_x_values": baseline_x_values,
    }


def build_fct_data_from_network_results(network_results, fct_metric, variable_name):
    """Build normalized FCT DataFrames for one metric from pre-collected network data."""
    fct_data = {}
    baseline_fct = {}
    x_values = network_results[0]["baseline_x_values"] if network_results else []

    for network_result in network_results:
        network_idx = network_result["network_idx"]
        network = network_result["network"]
        if "results_by_metric" in network_result:
            results = network_result["results_by_metric"][fct_metric]
        else:
            results = network_result["results"]

        table_data = {"Size": x_values}
        for variable, size_dict in results.items():
            column_name = f"{variable_name.capitalize()} {variable}"

            if not size_dict:
                print(f"No data for {network} at {variable_name}={variable}. Leaving {column_name} empty.")
                table_data[column_name] = [None] * len(x_values)
                continue

            averaged_values = []
            for size in x_values:
                if size not in size_dict:
                    averaged_values.append(None)
                    continue

                avg_value = sum(size_dict[size]) / len(size_dict[size])
                if network_idx == 0:
                    baseline_fct.setdefault(variable, {})[size] = avg_value
                    averaged_values.append(avg_value)
                elif variable in baseline_fct and size in baseline_fct[variable]:
                    averaged_values.append(avg_value / baseline_fct[variable][size])
                else:
                    print(f"Warning: Size {size} not found in baseline FCT. Skipping normalization.")
                    averaged_values.append(None)

            table_data[column_name] = averaged_values

        fct_data[network] = pd.DataFrame(table_data)

    return fct_data


def _worker_count(networks, max_workers=None):
    if max_workers is not None:
        return max_workers
    return min(len(networks), os.cpu_count() or len(networks))


def collect_all_metric_network_results(
    file_template,
    networks,
    metrics,
    variable_name,
    variable_values,
    seeds,
    *,
    alternate_file_templates=None,
    max_load_by_network=None,
    max_workers=None,
    verbose=False,
):
    """Read each network once and collect all requested FCT metrics."""
    alternate_file_templates = alternate_file_templates or {}
    max_load_by_network = max_load_by_network or {}

    with ProcessPoolExecutor(max_workers=_worker_count(networks, max_workers)) as executor:
        futures = [
            executor.submit(
                collect_network_all_fct_results_for_process,
                file_template,
                alternate_file_templates,
                max_load_by_network,
                network_idx,
                network,
                metrics,
                variable_name,
                variable_values,
                seeds,
                verbose,
            )
            for network_idx, network in enumerate(networks)
        ]
        return sorted((future.result() for future in futures), key=lambda item: item["network_idx"])


def compute_fct_data_average(
    file_template,
    networks,
    fct_metric,
    variable_name,
    variable_values,
    seeds,
    *,
    alternate_file_templates=None,
    max_load_by_network=None,
    max_workers=None,
    verbose=False,
):
    """Average FCT data across seeds and normalize non-baseline networks."""
    alternate_file_templates = alternate_file_templates or {}
    max_load_by_network = max_load_by_network or {}

    with ProcessPoolExecutor(max_workers=_worker_count(networks, max_workers)) as executor:
        futures = [
            executor.submit(
                collect_network_fct_results_for_process,
                file_template,
                alternate_file_templates,
                max_load_by_network,
                network_idx,
                network,
                fct_metric,
                variable_name,
                variable_values,
                seeds,
                verbose,
            )
            for network_idx, network in enumerate(networks)
        ]
        network_results = sorted((future.result() for future in futures), key=lambda item: item["network_idx"])

    return build_fct_data_from_network_results(network_results, fct_metric, variable_name)


def run_fct_plot(
    fct_metric,
    *,
    exp_name,
    file_template,
    networks,
    variable_name,
    variable_values,
    seeds,
    labels,
    xticks,
    xlim,
    plot_fct_results_func,
    display_func=None,
    display_network="opera_ecmp",
    alternate_file_templates=None,
    max_load_by_network=None,
    max_workers=None,
    verbose=False,
):
    """Compute averaged FCT data and plot one metric."""
    fig_name = f"{fct_metric}_{exp_name}"
    fct_results_df = compute_fct_data_average(
        file_template,
        networks,
        fct_metric,
        variable_name,
        variable_values,
        seeds,
        alternate_file_templates=alternate_file_templates,
        max_load_by_network=max_load_by_network,
        max_workers=max_workers,
        verbose=verbose,
    )

    if display_func is not None and display_network is not None and display_network in fct_results_df:
        display_func(fct_results_df[display_network])

    plot_fct_results_func(
        fct_results_df,
        variable_name,
        variable_values,
        labels,
        fct_metric,
        fig_name,
        xticks,
        xlim,
    )
    return fct_results_df


def plot_precomputed_fct_metric(
    network_results,
    fct_metric,
    *,
    exp_name,
    variable_name,
    variable_values,
    labels,
    xticks,
    xlim,
    plot_fct_results_func,
    display_func=None,
    display_network="opera_ecmp",
):
    """Plot one FCT metric from pre-collected network data."""
    fig_name = f"{fct_metric}_{exp_name}"
    fct_results_df = build_fct_data_from_network_results(network_results, fct_metric, variable_name)

    if display_func is not None and display_network is not None and display_network in fct_results_df:
        display_func(fct_results_df[display_network])

    plot_fct_results_func(
        fct_results_df,
        variable_name,
        variable_values,
        labels,
        fct_metric,
        fig_name,
        xticks,
        xlim,
    )
    return fct_results_df


def run_all_metrics(
    *,
    exp_name,
    file_template,
    networks,
    metrics,
    variable_name,
    variable_values,
    seeds,
    labels,
    xticks,
    xlim,
    plot_fct_results_func,
    display_func=None,
    display_network="opera_ecmp",
    alternate_file_templates=None,
    max_load_by_network=None,
    max_workers=None,
    verbose=False,
):
    """Run all requested FCT metrics after reading logs once."""
    network_results = collect_all_metric_network_results(
        file_template,
        networks,
        metrics,
        variable_name,
        variable_values,
        seeds,
        alternate_file_templates=alternate_file_templates,
        max_load_by_network=max_load_by_network,
        max_workers=max_workers,
        verbose=verbose,
    )
    return {
        metric: plot_precomputed_fct_metric(
            network_results,
            metric,
            exp_name=exp_name,
            variable_name=variable_name,
            variable_values=variable_values,
            labels=labels,
            xticks=xticks,
            xlim=xlim,
            plot_fct_results_func=plot_fct_results_func,
            display_func=display_func,
            display_network=display_network,
        )
        for metric in metrics
    }
