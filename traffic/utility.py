import numpy as np

def get_flow_mat(probabilities, srcdst, workload_, Gbps_rate, loadfrac0, H_active, totaltime):

    linkrate = Gbps_rate * 10e8 / 8  # bytes per second

    probabilities = np.array(probabilities)

    # Normalize the probabilities to ensure they sum to 1
    probabilities /= probabilities.sum()

    # Calculate the cumulative distribution function (CDF) using np.cumsum
    tmcdf = np.cumsum(probabilities)

    print(f"len(srcdst) = {len(srcdst)}")
    print(f"tmcdf = {tmcdf}") 
    print(f"len tmcdf = {len(tmcdf)}")

    # Load flow size distribution from CSV
    if workload_ == 'DM':
        flowdis_data = np.loadtxt('../_flow_dis/DM.csv', delimiter=',')
    elif workload_ == 'HD':
        flowdis_data = np.loadtxt('../_flow_dis/HD.csv', delimiter=',')
    elif workload_ == 'HD_10x':
        flowdis_data = np.loadtxt('../_flow_dis/HD_10x.csv', delimiter=',')
    elif workload_ == 'WS':
        flowdis_data = np.loadtxt('../_flow_dis/WS.csv', delimiter=',')
    else:
        raise ValueError('Unknown workload specified')

    flowsize = flowdis_data[:, 0]
    flowcdf = flowdis_data[:, 1]

    print(f"data = {flowdis_data} {type(flowdis_data)}")
    print(f"flowsize = {flowsize}")
    print(f"flowcdf = {flowcdf}")

    
    avg_flowsize = np.sum(flowsize[1:] * np.diff(flowcdf))  # bytes/flow

    lambda_host_max = linkrate / avg_flowsize  # flows/second per host
    lambda_host = loadfrac0 * lambda_host_max  # flows/second for each host

    lambda_network = H_active * lambda_host  # flows/second for the entire network

    nflows_est = int(np.ceil(lambda_network * totaltime))
    flowmat1 = np.zeros((nflows_est, 4), dtype=np.int64)

    print('Getting PRIO flow start times...')
    crt_time = 0
    cnt = 0
    while crt_time < totaltime:
        next_time = -np.log(1 - np.random.rand()) / lambda_network
        crt_time += next_time
        if cnt >= flowmat1.shape[0]:
            flowmat1 = np.vstack([flowmat1, np.zeros((flowmat1.shape[0], 4), dtype=np.int64)])
        flowmat1[cnt, 3] = int(crt_time * 1e9)  # nanoseconds
        cnt += 1

    flowmat1 = flowmat1[:cnt]


    print('Getting PRIO flow sizes...')
    randvect = np.random.rand(flowmat1.shape[0])
    indices = np.searchsorted(flowcdf, randvect)
    flowmat1[:, 2] = flowsize[indices]

    print('Getting PRIO flow sources & destinations...')
    randvect = np.random.rand(flowmat1.shape[0])
    indices = np.searchsorted(tmcdf, randvect)
    print(f"indices = {indices}")
    flowmat1[:, 0:2] = srcdst[indices]


    return flowmat1

def write_to_htsim_file(flowmat, filename):
    with open(filename, 'w') as f:
        for idx, row in enumerate(flowmat):
            if idx == len(flowmat)-1:
                f.write(f"{row[0]} {row[1]} {row[2]} {row[3]} {idx}")
            else:
                f.write(f"{row[0]} {row[1]} {row[2]} {row[3]} {idx}\n")
    print(f"Data written to {filename}")


    
if __name__ == "__main__":
    pass