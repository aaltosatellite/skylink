import sys
import setuptools

with open("README.md", "r", encoding="utf-8") as fh:
    long_description = fh.read()

setuptools.setup(
    name="skylink",
    version="0.0.1",
    author="Aalto Satellites",
    author_email="petri.niemela@aalto.fi",
    description="Skylink protocol parser",
    long_description=long_description,
    long_description_content_type="text/markdown",
    url="https://github.com/aaltosatellite/skylink",
    packages=setuptools.find_packages(),
    python_requires='>=3.7',
    install_requires=[
        "construct",
        "blake3",
    ],

)

