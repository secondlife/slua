"""Root exception for everything raised by slua_bundle.

Catch `BundleError` to handle any user-surfaceable error the bundler,
parser, resolver, or extractor can raise. Specific subclasses live next
to the code that raises them.
"""

from __future__ import annotations


class BundleError(Exception):
    pass
