
def getPdoName(node):
    name = node.xpathEval("Name")[0].content
    name = name.replace(" ", "")
    return name

def getEntryName(node):
    name= node.xpathEval("Name")[0].content
    name= name.replace(" ", "")
    return name

def parseInt(text):
    if text.startswith("#x") or text.startswith("0x"):
        return int(text.replace("#x", ""), 16)
    else:
        return int(text)

def hasEntryName(node):
    try:
        name= node.xpathEval("Name")[0].content
        subindex = node.xpathEval("SubIndex")[0].content
    except:
        return False
    return True
