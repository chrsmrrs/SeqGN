import sys

sys.path.insert(0, '..')
sys.path.insert(0, '.')

from graph_tool.all import *
from torch_geometric.datasets import TUDataset

import auxiliarymethods.datasets as dp
import preprocessing as pre

import os.path as osp
import numpy as np
from torch.nn import Sequential, Linear, ReLU
from torch_geometric.nn import global_mean_pool, GINConv

from torch_geometric.data import (InMemoryDataset, Data)
from torch_geometric.data import DataLoader

import torch
import torch.nn.functional as F

from aux import compute_k_s_tuple_graph_fast


class TUD_3_1(InMemoryDataset):
    def __init__(self, root, transform=None, pre_transform=None,
                 pre_filter=None):
        super(TUD_3_1, self).__init__(root, transform, pre_transform, pre_filter)
        self.data, self.slices = torch.load(self.processed_paths[0])

    @property
    def raw_file_names(self):
        return "TUD_2f_1tetrtgth"

    @property
    def processed_file_names(self):
        return "TUD_2f_erget1th"

    def download(self):
        pass

    def process(self):
        data_list = []

        indices_train = []
        indices_val = []
        indices_test = []


        indices_test = list(range(0,1000))

        # infile = open("val_al_10.index", "r")
        # for line in infile:
        #     indices_val = line.split(",")
        #     indices_val = [int(i) for i in indices_val]
        #
        # infile = open("train_al_10.index", "r")
        # for line in infile:
        #     indices_train = line.split(",")
        #     indices_train = [int(i) for i in indices_train]

        targets = dp.get_dataset("ZINC_test", multigregression=False)
        tmp_1 = targets[indices_train].tolist()
        tmp_2 = targets[indices_val].tolist()
        tmp_3 = targets[indices_test].tolist()
        targets = tmp_1
        targets.extend(tmp_2)
        targets.extend(tmp_3)

        node_labels = pre.get_all_node_labels_zinc_3_2("ZINC_test", True, True, indices_train, indices_val, indices_test)

        #matrices = pre.get_all_matrices_3_2("alchemy_full", indices_train)
        #matrices.extend(pre.get_all_matrices_3_2("alchemy_full", indices_val))
        matrices = pre.get_all_matrices_3_2("ZINC_test", indices_test)

        for i, m in enumerate(matrices):
            edge_index_1 = torch.tensor(matrices[i][0]).t().contiguous()
            edge_index_2 = torch.tensor(matrices[i][1]).t().contiguous()
            edge_index_3 = torch.tensor(matrices[i][2]).t().contiguous()

            data = Data()
            data.edge_index_1 = edge_index_1
            data.edge_index_2 = edge_index_2
            data.edge_index_3 = edge_index_3

            #one_hot = np.eye(4529)[node_labels[i]]
            data.x = torch.from_numpy(np.array(node_labels[i])).to(torch.float)
            data.y = data.y = torch.from_numpy(np.array([targets[i]])).to(torch.float)

            data_list.append(data)

        data, slices = self.collate(data_list)
        torch.save((data, slices), self.processed_paths[0])

class MyData(Data):
    def __inc__(self, key, value, *args, **kwargs):
        return self.num_nodes if key in [
            'edge_index_1', 'edge_index_2', 'edge_index_3'
        ] else 0


class MyTransform(object):
    def __call__(self, data):
        new_data = MyData()
        for key, item in data:
            new_data[key] = item
        return new_data


class NetGIN(torch.nn.Module):
    def __init__(self, dim):
        super(NetGIN, self).__init__()

        num_features = 4529


        nn1_1 = Sequential(Linear(num_features, dim), ReLU(), Linear(dim, dim))
        nn1_2 = Sequential(Linear(num_features, dim), ReLU(), Linear(dim, dim))
        nn1_3 = Sequential(Linear(num_features, dim), ReLU(), Linear(dim, dim))
        self.conv1_1 = GINConv(nn1_1, train_eps=True)
        self.conv1_2 = GINConv(nn1_2, train_eps=True)
        self.conv1_3 = GINConv(nn1_3, train_eps=True)
        self.bn1 = torch.nn.BatchNorm1d(dim)
        self.mlp_1 = Sequential(Linear(3 * dim, dim), ReLU(), Linear(dim, dim))

        nn2_1 = Sequential(Linear(dim, dim), ReLU(), Linear(dim, dim))
        nn2_2 = Sequential(Linear(dim, dim), ReLU(), Linear(dim, dim))
        nn2_3 = Sequential(Linear(dim, dim), ReLU(), Linear(dim, dim))
        self.conv2_1 = GINConv(nn2_1, train_eps=True)
        self.conv2_2 = GINConv(nn2_2, train_eps=True)
        self.conv2_3 = GINConv(nn2_3, train_eps=True)
        self.bn2 = torch.nn.BatchNorm1d(dim)
        self.mlp_2 = Sequential(Linear(3 * dim, dim), ReLU(), Linear(dim, dim))

        nn3_1 = Sequential(Linear(dim, dim), ReLU(), Linear(dim, dim))
        nn3_2 = Sequential(Linear(dim, dim), ReLU(), Linear(dim, dim))
        nn3_3 = Sequential(Linear(dim, dim), ReLU(), Linear(dim, dim))
        self.conv3_1 = GINConv(nn3_1, train_eps=True)
        self.conv3_2 = GINConv(nn3_2, train_eps=True)
        self.conv3_3 = GINConv(nn3_3, train_eps=True)
        self.bn3 = torch.nn.BatchNorm1d(dim)
        self.mlp_3 = Sequential(Linear(3 * dim, dim), ReLU(), Linear(dim, dim))

        nn4_1 = Sequential(Linear(dim, dim), ReLU(), Linear(dim, dim))
        nn4_2 = Sequential(Linear(dim, dim), ReLU(), Linear(dim, dim))
        nn4_3 = Sequential(Linear(dim, dim), ReLU(), Linear(dim, dim))
        self.conv4_1 = GINConv(nn4_1, train_eps=True)
        self.conv4_2 = GINConv(nn4_2, train_eps=True)
        self.conv4_3 = GINConv(nn4_3, train_eps=True)
        self.bn4 = torch.nn.BatchNorm1d(dim)
        self.mlp_4 = Sequential(Linear(3 * dim, dim), ReLU(), Linear(dim, dim))

        self.fc1 = Linear(4 * dim, dim)
        self.fc2 = Linear(dim, dim)
        self.fc3 = Linear(dim, dim)
        self.fc4 = Linear(dim, 1)

    def forward(self, data):
        x = data.x

        x = data.x
        x = x.long()

        x_new = torch.zeros(x.size(0), 4529).to(device)
        x_new[range(x_new.shape[0]), x.view(1, x.size(0))] = 1

        x = x_new

        x_1 = F.relu(self.conv1_1(x, data.edge_index_1))
        x_2 = F.relu(self.conv1_2(x, data.edge_index_2))
        x_3 = F.relu(self.conv1_3(x, data.edge_index_3))
        x_1_r = self.mlp_1(torch.cat([x_1, x_2, x_3], dim=-1))
        x_1_r = self.bn1(x_1_r)

        x_1 = F.relu(self.conv2_1(x_1_r, data.edge_index_1))
        x_2 = F.relu(self.conv2_2(x_1_r, data.edge_index_2))
        x_3 = F.relu(self.conv2_3(x_1_r, data.edge_index_3))
        x_2_r = self.mlp_2(torch.cat([x_1, x_2, x_3], dim=-1))
        x_2_r = self.bn2(x_2_r)

        x_1 = F.relu(self.conv3_1(x_2_r, data.edge_index_1))
        x_2 = F.relu(self.conv3_2(x_2_r, data.edge_index_2))
        x_3 = F.relu(self.conv3_3(x_2_r, data.edge_index_3))
        x_3_r = self.mlp_3(torch.cat([x_1, x_2, x_3], dim=-1))
        x_3_r = self.bn3(x_3_r)

        x_1 = F.relu(self.conv4_1(x_3_r, data.edge_index_1))
        x_2 = F.relu(self.conv4_2(x_3_r, data.edge_index_2))
        x_3 = F.relu(self.conv4_3(x_3_r, data.edge_index_3))
        x_4_r = self.mlp_4(torch.cat([x_1, x_2, x_3], dim=-1))
        x_4_r = self.bn4(x_4_r)

        x = torch.cat([x_1_r, x_2_r, x_3_r, x_4_r], dim=-1)
        x = global_mean_pool(x, data.batch)

        x = F.relu(self.fc1(x))
        x = F.relu(self.fc2(x))
        x = F.relu(self.fc3(x))
        x = self.fc4(x)
        return x.view(-1)


plot_all = []
results = []

for _ in range(5):
    device = torch.device('cuda' if torch.cuda.is_available() else 'cpu')
    plot_it = []
    path = osp.join(osp.dirname(osp.realpath(__file__)), '.', 'data', 'tetstktffgte')
    dataset = TUD_3_1(path, transform=MyTransform())

    train_dataset = dataset[0:800]
    val_dataset = dataset[800:900]
    test_dataset = dataset[900:]

    batch_size = 25
    train_loader = DataLoader(train_dataset, batch_size=batch_size, shuffle=True)
    val_loader = DataLoader(val_dataset, batch_size=batch_size, shuffle=True)
    test_loader = DataLoader(test_dataset, batch_size=batch_size, shuffle=True)

    model = NetGIN(256).to(device)
    optimizer = torch.optim.Adam(model.parameters(), lr=0.001)
    scheduler = torch.optim.lr_scheduler.ReduceLROnPlateau(optimizer, mode='min',
                                                           factor=0.5, patience=5,
                                                           min_lr=0.0000001)


    def train():
        model.train()
        loss_all = 0

        lf = torch.nn.L1Loss()

        for data in train_loader:
            data = data.to(device)
            optimizer.zero_grad()
            loss = lf(model(data), data.y)
            loss.backward()
            loss_all += loss.item() * data.num_graphs
            optimizer.step()
        return loss_all / len(train_loader.dataset)


    def test(loader):
        model.eval()
        error = 0

        for data in loader:
            data = data.to(device)
            error += (model(data) - data.y).abs().sum().item()  # MAE
        return error / len(loader.dataset)


    best_val_error = None
    for epoch in range(1, 201):
        lr = scheduler.optimizer.param_groups[0]['lr']
        loss = train()
        val_error = test(val_loader)
        scheduler.step(val_error)

        if best_val_error is None or val_error <= best_val_error:
            test_error = test(test_loader)
            best_val_error = val_error

        print('Epoch: {:03d}, LR: {:.7f}, Loss: {:.7f}, Validation MAE: {:.7f}, '
              'Test MAE: {:.7f}'.format(epoch, lr, loss, val_error, test_error))

        if lr < 0.000001:
            print("Converged.")
            break

    results.append(test_error)

print("########################")
print(results)
results = np.array(results)
print(results.mean(), results.std())

