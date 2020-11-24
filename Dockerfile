FROM ubuntu:18.04

COPY install_deps.sh /app/
RUN /app/install_deps.sh

COPY CMakeLists.txt run_linter.sh CPPLINT.cfg /app/
COPY src /app/src
RUN mkdir -p /app/build/Release

WORKDIR /app/
RUN ./run_linter.sh

WORKDIR /app/build/Release

RUN cmake ../.. -DCMAKE_BUILD_TYPE=Release
RUN make -j4

RUN ctest -V

COPY examples /app/examples
WORKDIR /app

RUN pytest
