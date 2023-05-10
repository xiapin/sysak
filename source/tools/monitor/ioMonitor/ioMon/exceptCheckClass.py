# -*- coding: utf-8 -*-

import sys
import string


class exceptCheckClass():
    def __init__(self, window):
        self.window = int(window) if window is not None else 100
        self._exceptChkDicts = {}

    def addItem(self, key):
        exceptChkItem = {
            'baseThresh': {
                'nrSample': 0,
                'moveWinData': [],
                'curWinMinVal': sys.maxsize,
                'curWinMaxVal': 0,
                'moveAvg': 0,
                'thresh': 0},
            'compensation': {
                'thresh': 0,
                'shouldUpdThreshComp': True,
                'decRangeThreshAvg': 0,
                'decRangeCnt': 0,
                'minStableThresh': sys.maxsize,
                'maxStableThresh': 0,
                'stableThreshAvg': 0,
                'nrStableThreshSample': 0},
            'dynTresh': sys.maxsize,
            'usedWin': 0}
        self._exceptChkDicts.setdefault(key, exceptChkItem)

    # The sliding window calculates the basic threshold, through which the spikes
    # and burrs in the IO indicators can be screened. The calculation idea is as
    # follows:
    # 1. take 100 data as a group for calculation (calculate 1 ~ 100 data for the
    #    first time, 2 ~ 101 for the second time, 3 ~ 102 for the third time, and
    #    so on), and calculate the average value mavg of 100 data in the current
    #    window
    # 2. obtain the maximum value Max and minimum value min of 100 data, then record
    #    the thresh (MAX((max-mavg),(mavg-min))) each time, and calculate the average
    #    value(threshavg) of all thresh at this time each time, taking threshavg as
    #    the basic threshold for this time
    # 3. The next basic threshold follows steps 1, 2, and so on
    def _calcBaseThresh(self, key, e):
        exceptChkDict = self._exceptChkDicts[key]
        bt = exceptChkDict['baseThresh']
        thresh = None

        bt['nrSample'] += 1
        if bt['nrSample'] >= self.window:
            if len(bt['moveWinData']) < self.window:
                bt['moveWinData'].append(e)
            else:
                bt['moveWinData'][exceptChkDict['usedWin'] % self.window] = e
            moveAvg = float(
                format(sum(bt['moveWinData']) / float(self.window), '.1f'))

            # Find the min and max values of this window so far
            maxVal = max(bt['curWinMaxVal'], e)
            minVal = min(bt['curWinMinVal'], e)
            nrThreshSample = bt['nrSample'] + 1 - self.window
            thresh = float(
                format(max(maxVal - moveAvg, moveAvg - minVal), '.1f'))
            # Calculate base threshold
            threshAvg = float(format(
                (bt['thresh'] * (nrThreshSample - 1) + thresh) / nrThreshSample,
                '.3f'))
            bt['thresh'] = threshAvg
            bt['moveAvg'] = moveAvg
            bt['curWinMaxVal'] = maxVal
            bt['curWinMinVal'] = minVal

            exceptChkDict['usedWin'] += 1
            if exceptChkDict['usedWin'] >= self.window:
                # the next window, set min and Max to 0
                bt['curWinMaxVal'] = 0
                bt['curWinMinVal'] = sys.maxsize
                exceptChkDict['usedWin'] = 0
        else:
            # Here, only the first window will enter to ensure that
            # the data in one window is accumulated
            bt['moveWinData'].append(e)
            bt['curWinMaxVal'] = max(bt['curWinMaxVal'], e)
            bt['curWinMinVal'] = min(bt['curWinMinVal'], e)
            exceptChkDict['usedWin'] += 1
        return thresh

    # Called by _calcCompThresh to calculate the compensation value
    # under normal steady state
    def _calcStableThresh(self, ct, curBaseThresh, curThresh):
        # Discard points exceeding (base-threshold / 10)
        avg = ct['decRangeThreshAvg']
        if (curThresh - avg) < ((curBaseThresh - avg) / 10.0):
            tSum = ct['stableThreshAvg'] * \
                ct['nrStableThreshSample'] + curThresh
            ct['nrStableThreshSample'] += 1
            ct['stableThreshAvg'] = tSum / ct['nrStableThreshSample']
            ct['minStableThresh'] = min(ct['minStableThresh'], curThresh)
            ct['maxStableThresh'] = max(ct['maxStableThresh'], curThresh)
            # 1.5 windows of stable data have been counted,
            # which can be used as normal threshold compensation value
            if ct['nrStableThreshSample'] >= (self.window * 1.5):
                ct['thresh'] = \
                    max(ct['stableThreshAvg'] - ct['minStableThresh'],
                        ct['maxStableThresh'] - ct['stableThreshAvg'])
                ct['shouldUpdThreshComp'] = False
                ct['minStableThresh'] = sys.maxsize
                ct['maxStableThresh'] = 0
                ct['stableThreshAvg'] = ct['decRangeThreshAvg'] = 0
                ct['nrStableThreshSample'] = ct['decRangeCnt'] = 0

    # Calculate the threshold compensation value and superimpose this value
    # on the basic threshold to eliminate false alarms
    def _calcCompThresh(self, key, lastBaseThresh, curThresh):
        exceptChkDict = self._exceptChkDicts[key]
        curBaseThresh = exceptChkDict['baseThresh']['thresh']
        ct = exceptChkDict['compensation']

        # It is not confirmed whether the current state is constant
        # (constant state is defined as IO index fluctuation, which is stable)
        # 1. the max basic threshold of this window is the compensation value
        # 2. enter a new window to reset to the current basic threshold
        if ct['shouldUpdThreshComp'] == True and \
                (ct['thresh'] < curBaseThresh or exceptChkDict['usedWin'] == 0):
            ct['thresh'] = curBaseThresh

        # Continuous monotonic decreasing, constant steady state,
        # constant compensation threshold inferred
        if curBaseThresh < lastBaseThresh:
            tSum = ct['decRangeThreshAvg'] * ct['decRangeCnt'] + curThresh
            ct['decRangeCnt'] += 1
            ct['decRangeThreshAvg'] = tSum / ct['decRangeCnt']
            # The monotonic decline has continued for 1.5 windows,
            # indicating that IO pressure may return to normality
            if ct['decRangeCnt'] >= (self.window * 1.5):
                self._calcStableThresh(ct, curBaseThresh, curThresh)
        else:
            # As long as the basic threshold curve is not
            # continuously monotonically decreasing,
            # reset to 0 and make statistics again
            ct['minStableThresh'] = sys.maxsize
            ct['maxStableThresh'] = 0
            ct['stableThreshAvg'] = ct['decRangeThreshAvg'] = 0
            ct['nrStableThreshSample'] = ct['decRangeCnt'] = 0

    # Update the dynamic threshold of the corresponding indicator type
    # and call it after collecting the IO indicators. The key is await,
    # util, IOPs, BPS, etc
    def updateDynThresh(self, key, e):
        exceptChkDict = self._exceptChkDicts[key]
        bt = exceptChkDict['baseThresh']
        ct = exceptChkDict['compensation']
        lastBaseThresh = bt['thresh']

        curThresh = self._calcBaseThresh(key, e)
        if curThresh is not None:
            self._calcCompThresh(key, lastBaseThresh, curThresh)
            exceptChkDict['dynTresh'] = \
                bt['thresh'] + bt['moveAvg'] + ct['thresh']

    # Turn off the threshold compensation of the corresponding indicators.
    # Generally, when it is detected that the IO util exceeds 20%,
    # it will be disabled according to the situation of each indicator
    def disableThreshComp(self, key):
        exceptChkDict = self._exceptChkDicts[key]
        ct = exceptChkDict['compensation']
        bt = exceptChkDict['baseThresh']

        #if exceptChkDict['dynTresh'] == sys.maxsize:
        #    return

        if ct['shouldUpdThreshComp'] == True:
            ct['shouldUpdThreshComp'] = False
            exceptChkDict['dynTresh'] = bt['thresh'] + bt['moveAvg']
            ct['thresh'] = 0.000001


    def getNrDataSample(self, key):
        return self._exceptChkDicts[key]['baseThresh']['nrSample']

    # Get the dynamic threshold of the corresponding indicator type,
    # call it after collecting the IO indicators, and judge whether
    # the indicators are abnormal. The key is await, util, IOPs, BPS, etc
    def getDynThresh(self, key):
        return self._exceptChkDicts[key]['dynTresh']
