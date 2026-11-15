"""GBA tutorial button labels, without changing words such as 'Crash'."""
def adapt(streams):
    changed = []
    for number, rows in enumerate(streams):
        for i in range(1, len(rows) - 1):
            code = rows[i][1]
            if code in (ord('C'), ord('D')) and rows[i - 1][1] == 32 and rows[i + 1][1] in (32, 44, 46):
                rows[i][1] = ord('R') if code == ord('C') else ord('L')
                changed.append((number, i, chr(code), chr(rows[i][1])))
    return changed
