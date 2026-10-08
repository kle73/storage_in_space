# Quantum Network Simulation Module for ns-3

# Getting Started

1. Clone the ns-3 mainline code

    ```bash
    git clone -b ns-3.45 https://gitlab.com/nsnam/ns-3-dev.git
    ```

2. Change into the contrib directory

    ```bash
    cd contrib
    ```

3. Clone the Quantum Network Simulation Module

    ```bash
    git clone https://gitlab.com/nitk-surathkal/quantum.git
    ```

4. Check if the patches for the additional changes succeed

    ```bash
    cd ../
    ```

    ```bash
    git apply --check contrib/quantum/patches/ns-3.45.patch
    ```

    If you do not face any errors, apply the patch

    ```bash
    git apply contrib/quantum/patches/ns-3.45.patch
    ```

5. Configure ns-3 and build it. Ensure the following libraries are installed:
    1. [qpp](https://github.com/softwareQinc/qpp)
    2. [cryptopp](https://github.com/weidai11/cryptopp)
    3. SQLite3

    ```bash
    ./ns3 configure --enable-examples --enable-qpp --enable-crypto
    ```

    ```bash
    ./ns3 build
    ```

6. Run the quantum network examples

    ```bash
    ./ns3 run qkd-example
    ```

    ```bash
    ./ns3 run qkd-eavesdropper-example
    ```

    ```bash
    ./ns3 run qkd-application-first-example
    ```
# Acknowledgment

This project is supported under the Q-Pragathi initiative of the Quantum Research Park (QuRP) at the Indian Institute of Science (IISc). We gratefully acknowledge the funding and support provided by the Karnataka Innovation and Technology Society (KITS), K-Tech, Government of Karnataka, India.
