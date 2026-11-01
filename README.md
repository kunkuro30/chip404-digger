# chip404-digger

Parallel PoW nonce search for the Chip404 free mint on BSC.

Find `nonce` such that `keccak256(salt || wallet || nonce)` (84-byte packed payload)
has at least `bits` leading zero bits, matching the mint contract's `mint(uint256 nonce)`
proof check at the current `weightNow()`.

Salt and wallet are public on-chain data. The search is pure computation;
signing and the mint transaction stay on the owner's agent machine.
