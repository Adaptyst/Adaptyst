# SPDX-FileCopyrightText: 2026 CERN
# SPDX-License-Identifier: LGPL-3.0-or-later

FROM gitlab-registry.cern.ch/adaptyst/gentoo-fp:latest
RUN mkdir -p /root/adaptyst
COPY . /root/adaptyst/
RUN emerge dev-build/ninja net-misc/wget sys-process/numactl dev-cpp/cli11 \
    dev-cpp/nlohmann_json dev-libs/boost app-arch/libarchive \
    dev-python/pybind11 dev-python/pip dev-libs/poco && \
    pip install dace pyyaml && cd /root/adaptyst && mkdir build && \
    cd build && cmake .. -G Ninja && cmake --build . && cmake --install . && \
    cd ../.. && rm -rf adaptyst && emerge --deselect dev-build/ninja \
    dev-cpp/cli11 dev-python/pybind11 dev-cpp/nlohmann_json && \
    emerge --depclean
RUN useradd -m gentoo-adaptyst
USER gentoo-adaptyst
