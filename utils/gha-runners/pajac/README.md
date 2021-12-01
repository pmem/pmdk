# PaJaC

PMDK-unittests as JunitXml asset Converter

This directory holds the script which is converting console output from PMDK tests to the JUnit XML report format.

Converter requires python3.

Example usage:

```
$ python3 converter.py --help

$ python3 converter.py input.file.path.log output.file.path.xml

```

## Unit tests

Converter is covered by basic set of unit tests.

To run tests, use the following command from main script's directory:

```
python3 -m unittest -v
```
