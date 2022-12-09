#!/usr/bin/python3
import os
import sys
import ftplib

testdir = "test"
outdir = os.path.join(testdir, "out")
datadir = os.path.join(testdir, "data")
imagedir = os.path.join(datadir, "images")

def test_invalid_login(port: int):
    client = __create_client(port)
    try:
        send_print("USER anonymous")
        client.login("anonymous", "anonymous")
    except Exception as err:
        recv_print(err.args[0])

def test_invalid_cwd(port: int):
    client = __create_client(port)
    send_print("USER cs317")
    recv_print(client.login('cs317', 'anonymous'))
    send_print("CWD " + datadir)
    recv_print(client.cwd(datadir))
    try:
        send_print("CWD " + datadir)
        client.cwd(datadir)
    except Exception as err:
        recv_print(err.args[0])
    client.close()

def test_cdup(port: int):
    client = __create_client(port)
    send_print("USER cs317")
    recv_print(client.login('cs317', 'anonymous'))
    send_print("CWD " + datadir)
    recv_print(client.cwd(datadir))
    send_print("CDUP")
    recv_print(client.cwd(".."))
    send_print("CDUP")
    recv_print(client.cwd(".."))
    try:
        send_print("CDUP")
        client.cwd("..")
    except Exception as err:
        recv_print(err.args[0])
    client.close()

def test_inaccessible_cwd1(port: int):
    client = __create_client(port)
    send_print("USER cs317")
    recv_print(client.login('cs317', 'anonymous'))
    dirname = "../"
    try:
        send_print("CWD " + dirname)
        client.cwd(dirname)
    except Exception as err:
        recv_print(err.args[0])
    client.close()

def test_inaccessible_cwd2(port: int):
    client = __create_client(port)
    send_print("USER cs317")
    recv_print(client.login('cs317', 'anonymous'))
    dirname = "/.."
    try:
        send_print("CWD " + dirname)
        client.cwd(dirname)
    except Exception as err:
        recv_print(err.args[0])
    client.close()

def test_inaccessible_cwd3(port: int):
    client = __create_client(port)
    send_print("USER cs317")
    recv_print(client.login('cs317', 'anonymous'))
    dirname = "./"
    try:
        send_print("CWD " + dirname)
        client.cwd(dirname)
    except Exception as err:
        recv_print(err.args[0])
    client.close()

def test_cwd_root(port: int):
    client = __create_client(port)
    send_print("USER cs317")
    recv_print(client.login('cs317', 'anonymous'))
    direntnames1 = [line.split()[1] for line in client.nlst()]
    send_print("CWD /")
    recv_print(client.cwd("/"))
    send_print("NLST")
    direntnames2 = [line.split()[1] for line in client.nlst()]
    assert direntnames1 == direntnames2
    recv_print("")
    for line in client.nlst():
        print(line)
    client.close()

def test_nlst_data(port: int):
    client = __create_client(port)
    send_print("USER cs317")
    recv_print(client.login('cs317', 'anonymous'))
    send_print("CWD " + datadir)
    recv_print(client.cwd(datadir))
    send_print("NLST")
    recv_print("")
    for line in client.nlst():
        print(line)
    client.close()

def test_retr_invalid(port: int):
    client = __create_client(port)
    send_print("USER cs317")
    recv_print(client.login('cs317', 'anonymous'))
    filepath = os.path.join(datadir, "foo")
    try:
        command = f"RETR {filepath}"
        send_print(command)
        client.retrlines(command, None)
    except Exception as err:
        recv_print(err.args[0])
    client.close()

def test_retr_inaccessible(port: int):
    client = __create_client(port)
    send_print("USER cs317")
    recv_print(client.login('cs317', 'anonymous'))
    filepath = os.path.join(datadir, "./answer.txt")
    try:
        command = f"RETR {filepath}"
        send_print(command)
        client.retrlines(command, None)
    except Exception as err:
        recv_print(err.args[0])
    client.close()

def test_retr_txt1(port: int):
    client = __create_client(port)
    send_print("USER cs317")
    recv_print(client.login('cs317', 'anonymous'))
    send_print("CWD " + datadir)
    recv_print(client.cwd(datadir))
    filename = "answer.txt"
    outpath = os.path.join(outdir, filename)
    with open(outpath, "w", encoding="utf-8") as f:
        command = f"RETR {filename}"
        send_print(command)
        recv_print(client.retrlines(command, f.write))
    client.close()
    print(f"Received {outpath}")

def test_retr_txt2(port: int):
    client = __create_client(port)
    send_print("USER cs317")
    recv_print(client.login('cs317', 'anonymous'))
    filename = "authors.txt"
    filepath = os.path.join("/" + datadir, filename)
    outpath = os.path.join(outdir, filename)
    with open(outpath, "wb") as f:
        command = f"RETR {filepath}"
        send_print(command)
        recv_print(client.retrbinary(command, f.write))
    client.close()
    print(f"Received {outpath}")

def test_retr_image(port: int):
    client = __create_client(port)
    send_print("USER cs317")
    recv_print(client.login('cs317', 'anonymous'))
    send_print("CWD " + imagedir)
    recv_print(client.cwd(imagedir))
    filename = "guin.jpg"
    outpath = os.path.join(outdir, filename)
    with open(outpath, "wb") as f:
        command = f"RETR {filename}"
        send_print(command)
        recv_print(client.retrbinary(command, f.write))
    client.close()
    print(f"Received {outpath}")

def __create_client(port: int):
    ftp = ftplib.FTP()
    try:
        recv_print(ftp.connect(host="localhost", port=port))
    except ConnectionRefusedError:
        print(f"Failed to connect to (localhost, {port})\n")
        sys.exit(0)
    return ftp

def send_print(msg: str):
    print(f"\u001B[34m--> {msg}\u001B[37m")

def recv_print(msg: str):
    print(f"\u001B[31m<-- {msg}\u001B[37m")

def get_port():
    if len(sys.argv) < 2:
        return -1
    return int(sys.argv[1]) if sys.argv[1].isdigit() else -1

def print_test_header(name: str):
    """Prints header for unittest of length headersize"""
    headersize = 50
    barsize = (headersize - len(name) - 2) // 2
    space = "" if 2 * (barsize + 1) + len(name) == headersize else " "
    headerbar = "=" * barsize
    print(f"\n\u001B[33m{headerbar} {name}{space} {headerbar}\u001B[37m")

def main():
    port = get_port()
    if port < 0:
        print(f"usage: python3 {testdir}/{sys.argv[0]} <port>")
        return
    print_test_header("Invalid login")
    test_invalid_login(port)
    print_test_header("Invalid CWD")
    test_invalid_cwd(port)
    print_test_header("CDUP")
    test_cdup(port)
    print_test_header("Inaccessible CWD 1")
    test_inaccessible_cwd1(port)
    print_test_header("Inaccessible CWD 2")
    test_inaccessible_cwd2(port)
    print_test_header("Inaccessible CWD 3")
    test_inaccessible_cwd3(port)
    print_test_header("CWD root")
    test_cwd_root(port)
    print_test_header("NLST data")
    test_nlst_data(port)
    print_test_header("RETR invalid")
    test_retr_invalid(port)
    print_test_header("RETR inaccessible")
    test_retr_inaccessible(port)
    print_test_header("RETR text file 1")
    test_retr_txt1(port)
    print_test_header("RETR text file 2")
    test_retr_txt2(port)
    print_test_header("RETR image file")
    test_retr_image(port)
    sys.stdout.write("\n")

if __name__ == "__main__":
    main()
