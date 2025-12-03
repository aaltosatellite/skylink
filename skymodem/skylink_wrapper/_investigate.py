import os


dpath1 = "/home/elmore/old-SL/skylink/skymodem/"


def invstgt(dpath):
    assert os.path.isdir(dpath)
    dlist = os.listdir(dpath)
    for name in dlist:
        path = os.path.join(dpath, name)
        if not os.path.isfile(path):
            continue
        f = open(path)
        rd = f.read()
