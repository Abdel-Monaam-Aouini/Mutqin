package app.mutqin.ui

/** Which units of the expected and the heard reading differ from the other side. */
data class UnitDiff(
    val expectedDiffers: Set<Int>,
    val heardDiffers: Set<Int>,
)

/**
 * The classic edit alignment of two unit sequences: units that match stay out of the sets; a unit that was
 * replaced, missing or added is in the set of its side.
 */
fun diffUnits(
    expected: List<String>,
    heard: List<String>,
): UnitDiff {
    val n = expected.size
    val m = heard.size
    val cost = Array(n + 1) { IntArray(m + 1) }
    for (i in 0..n) cost[i][0] = i
    for (j in 0..m) cost[0][j] = j
    for (i in 1..n) {
        for (j in 1..m) {
            val same = if (expected[i - 1] == heard[j - 1]) 0 else 1
            cost[i][j] = minOf(cost[i - 1][j] + 1, cost[i][j - 1] + 1, cost[i - 1][j - 1] + same)
        }
    }
    val expectedDiffers = mutableSetOf<Int>()
    val heardDiffers = mutableSetOf<Int>()
    var i = n
    var j = m
    while (i > 0 || j > 0) {
        when {
            i > 0 && j > 0 && cost[i][j] == cost[i - 1][j - 1] + (if (expected[i - 1] == heard[j - 1]) 0 else 1) -> {
                if (expected[i - 1] != heard[j - 1]) {
                    expectedDiffers += i - 1
                    heardDiffers += j - 1
                }
                i--
                j--
            }

            i > 0 && cost[i][j] == cost[i - 1][j] + 1 -> {
                expectedDiffers += i - 1
                i--
            }

            else -> {
                heardDiffers += j - 1
                j--
            }
        }
    }
    return UnitDiff(expectedDiffers, heardDiffers)
}
