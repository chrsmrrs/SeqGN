import sys

sys.path.insert(0, '..')
sys.path.insert(0, '.')

import auxiliarymethods.datasets as dp
import preprocessing as pre

import os.path as osp
import numpy as np
import torch
from torch.nn import Sequential, Linear, ReLU
from torch_geometric.nn import global_mean_pool, GINConv

from torch_geometric.data import (InMemoryDataset, Data)
from torch_geometric.data import DataLoader
import torch.nn.functional as F


class ZINC(InMemoryDataset):
    def __init__(self, root, transform=None, pre_transform=None,
                 pre_filter=None):
        super(ZINC, self).__init__(root, transform, pre_transform, pre_filter)
        self.data, self.slices = torch.load(self.processed_paths[0])

    @property
    def raw_file_names(self):
        return "zinc10k"

    @property
    def processed_file_names(self):
        return "zinc10k"

    def download(self):
        pass

    def process(self):
        data_list = []

        targets = dp.get_dataset("ZINC_full")
        all = list(range(0, len(targets)))
        node_labels = pre.get_all_node_labels_connected_2("ZINC_full", True, True, all, [], [])
        node_labels_all = pre.get_all_node_labels_2("ZINC_full", True, True, all, [], [])

        matrices = pre.get_all_matrices_local_connected_2("ZINC_full", all)

        for i, m in enumerate(matrices):
            edge_index_1 = torch.tensor(matrices[i][0]).t().contiguous()
            edge_index_2 = torch.tensor(matrices[i][1]).t().contiguous()

            data = Data()
            data.edge_index_1 = edge_index_1
            data.edge_index_2 = edge_index_2

            one_hot = np.eye(242)[node_labels[i]]
            data.x = torch.from_numpy(one_hot).to(torch.float)

            one_hot = np.eye(652)[node_labels_all[i]]
            data.x_all = torch.from_numpy(one_hot).to(torch.float)

            n = one_hot.shape[0]
            data.num_all = n
            data.batch_all = torch.from_numpy(np.zeros(n)).to(torch.long)

            data.y = data.y = torch.from_numpy(np.array([targets[i]])).to(torch.float)

            data_list.append(data)

        data, slices = self.collate(data_list)
        torch.save((data, slices), self.processed_paths[0])


class MyData(Data):
    def __inc__(self, key, value):
        if key in ['edge_index_1', 'edge_index_2']:
            return self.num_nodes
        if key in ['batch_all']:
            return 1
        else:
            return 0


class MyTransform(object):
    def __call__(self, data):
        new_data = MyData()
        for key, item in data:
            new_data[key] = item
        return new_data


class NetGIN(torch.nn.Module):
    def __init__(self, dim):
        super(NetGIN, self).__init__()

        num_features = 242

        self.nn_all = Sequential(Linear(652, dim), torch.nn.BatchNorm1d(dim), ReLU(), Linear(dim, dim),
                           torch.nn.BatchNorm1d(dim), ReLU())

        nn1_1 = Sequential(Linear(num_features, dim), ReLU(), Linear(dim, dim))
        nn1_2 = Sequential(Linear(num_features, dim), ReLU(), Linear(dim, dim))
        self.conv1_1 = GINConv(nn1_1, train_eps=True)
        self.conv1_2 = GINConv(nn1_2, train_eps=True)
        self.bn1 = torch.nn.BatchNorm1d(dim)
        self.mlp_1 = Sequential(Linear(2 * dim, dim), ReLU(), Linear(dim, dim))

        nn2_1 = Sequential(Linear(dim, dim), ReLU(), Linear(dim, dim))
        nn2_2 = Sequential(Linear(dim, dim), ReLU(), Linear(dim, dim))
        self.conv2_1 = GINConv(nn2_1, train_eps=True)
        self.conv2_2 = GINConv(nn2_2, train_eps=True)
        self.bn2 = torch.nn.BatchNorm1d(dim)
        self.mlp_2 = Sequential(Linear(2 * dim, dim), ReLU(), Linear(dim, dim))

        nn3_1 = Sequential(Linear(dim, dim), ReLU(), Linear(dim, dim))
        nn3_2 = Sequential(Linear(dim, dim), ReLU(), Linear(dim, dim))
        self.conv3_1 = GINConv(nn3_1, train_eps=True)
        self.conv3_2 = GINConv(nn3_2, train_eps=True)
        self.bn3 = torch.nn.BatchNorm1d(dim)
        self.mlp_3 = Sequential(Linear(2 * dim, dim), ReLU(), Linear(dim, dim))

        nn4_1 = Sequential(Linear(dim, dim), ReLU(), Linear(dim, dim))
        nn4_2 = Sequential(Linear(dim, dim), ReLU(), Linear(dim, dim))
        self.conv4_1 = GINConv(nn4_1, train_eps=True)
        self.conv4_2 = GINConv(nn4_2, train_eps=True)
        self.bn4 = torch.nn.BatchNorm1d(dim)
        self.mlp_4 = Sequential(Linear(2 * dim, dim), ReLU(), Linear(dim, dim))

        nn5_1 = Sequential(Linear(dim, dim), ReLU(), Linear(dim, dim))
        nn5_2 = Sequential(Linear(dim, dim), ReLU(), Linear(dim, dim))
        self.conv5_1 = GINConv(nn5_1, train_eps=True)
        self.conv5_2 = GINConv(nn5_2, train_eps=True)
        self.bn5 = torch.nn.BatchNorm1d(dim)
        self.mlp_5 = Sequential(Linear(2 * dim, dim), ReLU(), Linear(dim, dim))

        nn6_1 = Sequential(Linear(dim, dim), ReLU(), Linear(dim, dim))
        nn6_2 = Sequential(Linear(dim, dim), ReLU(), Linear(dim, dim))
        self.conv6_1 = GINConv(nn6_1, train_eps=True)
        self.conv6_2 = GINConv(nn6_2, train_eps=True)
        self.bn6 = torch.nn.BatchNorm1d(dim)
        self.mlp_6 = Sequential(Linear(2 * dim, dim), ReLU(), Linear(dim, dim))

        self.fc1 = Linear(4 * dim + 652, dim)
        self.fc2 = Linear(dim, dim)
        self.fc3 = Linear(dim, dim)
        self.fc4 = Linear(dim, 1)

    def forward(self, data):
        x = data.x

        x_all = data.x_all

        x_1 = F.relu(self.conv1_1(x, data.edge_index_1))
        x_2 = F.relu(self.conv1_2(x, data.edge_index_2))
        x_1_r = self.mlp_1(torch.cat([x_1, x_2], dim=-1))
        x_1_r = self.bn1(x_1_r)

        x_1 = F.relu(self.conv2_1(x_1_r, data.edge_index_1))
        x_2 = F.relu(self.conv2_2(x_1_r, data.edge_index_2))
        x_2_r = self.mlp_2(torch.cat([x_1, x_2], dim=-1))
        x_2_r = self.bn2(x_2_r)

        x_1 = F.relu(self.conv3_1(x_2_r, data.edge_index_1))
        x_2 = F.relu(self.conv3_2(x_2_r, data.edge_index_2))
        x_3_r = self.mlp_3(torch.cat([x_1, x_2], dim=-1))
        x_3_r = self.bn3(x_3_r)

        x_1 = F.relu(self.conv4_1(x_3_r, data.edge_index_1))
        x_2 = F.relu(self.conv4_2(x_3_r, data.edge_index_2))
        x_4_r = self.mlp_4(torch.cat([x_1, x_2], dim=-1))
        x_4_r = self.bn4(x_4_r)

        # x_1 = F.relu(self.conv5_1(x_4_r, data.edge_index_1))
        # x_2 = F.relu(self.conv5_2(x_4_r, data.edge_index_2))
        # x_5_r = self.mlp_5(torch.cat([x_1, x_2], dim=-1))
        # x_5_r = self.bn5(x_5_r)
        #
        # x_1 = F.relu(self.conv6_1(x_5_r, data.edge_index_1))
        # x_2 = F.relu(self.conv6_2(x_5_r, data.edge_index_2))
        # x_6_r = self.mlp_6(torch.cat([x_1, x_2], dim=-1))
        # x_6_r = self.bn4(x_6_r)

        x = torch.cat([x_1_r, x_2_r, x_3_r, x_4_r], dim=-1)
        x = global_mean_pool(x, data.batch)
        #x_all = self.nn_all(x_all)
        x_all = global_mean_pool(x_all, data.batch_all)

        x = torch.cat([x, x_all], dim=-1)

        x = F.relu(self.fc1(x))
        x = F.relu(self.fc2(x))
        x = F.relu(self.fc3(x))
        x = self.fc4(x)
        return x.view(-1)


path = osp.join(osp.dirname(osp.realpath(__file__)), '.', 'data', 'ZINC')
dataset = ZINC(path, transform=MyTransform())
exit()

train_dataset = dataset[0:220011].shuffle()
val_dataset = dataset[225011:249456].shuffle()
test_dataset = dataset[220011:225011].shuffle()

batch_size = 25
train_loader = DataLoader(train_dataset, batch_size=batch_size, shuffle=True)
val_loader = DataLoader(val_dataset, batch_size=batch_size, shuffle=True)
test_loader = DataLoader(test_dataset, batch_size=batch_size, shuffle=True)

results = []
for _ in range(5):
    device = torch.device('cuda' if torch.cuda.is_available() else 'cpu')
    model = NetGIN(256).to(device)
    optimizer = torch.optim.Adam(model.parameters(), lr=0.001)
    scheduler = torch.optim.lr_scheduler.ReduceLROnPlateau(optimizer, mode='min',
                                                           factor=0.5, patience=15,
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
    for epoch in range(1, 501):
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

