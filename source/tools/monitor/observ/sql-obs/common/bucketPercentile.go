package common

var threshold = []uint64{
    1000000, 2000000, 3000000, 4000000, 6000000, 10000000,
    15000000, 22000000, 34000000, 51000000, 100000000,
    170000000, 250000000, 410000000, 750000000, 1000000000,
    1300000000, 1700000000, 2500000000, 3500000000, 5000000000,
    7000000000, 10000000000,
}

func BuildBucket() *[]uint64 {
    bucket := make([]uint64, len(threshold))
    for index := 0; index < len(threshold); index++ {
        bucket[index] = 0
    }
    return &bucket
}

func InsertBucket(value uint64, bucket *[]uint64) {
    for index, v := range threshold {
        if value <= v {
            (*bucket)[index] += 1
            break
        }
    }
}

func GetPercentile(bucket *[]uint64, percent uint64, count uint64) uint64 {
    count *= percent
    var sum uint64 = 0
    for index, value := range *bucket {
        sum += value * 100
        if sum >= count {
            return threshold[index]
        }
    }
    return threshold[len(threshold)-1]
}
