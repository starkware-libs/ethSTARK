import pytest


def pytest_addoption(parser):
    parser.addoption(
        '--flavor', action='store', default='Release',
        help='Build flavor to run test on: Release, RelWithDebInfo and Debug.'
    )


@pytest.fixture(scope='session')
def flavor(request):
    return request.config.getoption('--flavor')
