# Documentations

## API documentation (Doxygen)

To see Doxygen's API documentation open `doxygen/index.md` file.

## Project documentation

To see project documentation you need to build `.rst` source files with Sphinx tool.

Follow the steps below:

1. Install sphinx tool, e.g. with `pip`:

    ```shell
        pip3 install sphinx m2r2 sphinx_book_theme
    ```

2. Build `.rst` pages in the format you need, e.g. `html`:

    ```shell
        sphinx-build -b html ./source ./build
    ```
    If Running Sphinx shows error like below:
    ```
    Exception occurred:
        File "/usr/lib/python3/dist-packages/jinja2/loaders.py", line 163, in __init__
            self.searchpath = list(searchpath)
        TypeError: 'PosixPath' object is not iterable
    ```
    Update Jinja2 to latest version:
    ```shell
    pip install -U Jinja2
    ```

To see `html` built documentation open `./build/index.html` file.
