def format_param(x_str):
    x = eval(x_str)
    if x >= 10000:
        e = 0
        while x >= 10:
            x /= 10
            e += 1
        if abs(x - round(x)) < 1e-1:
            return "%de%d" % (int(round(x)), e)
        else:
            return "%.1fe%d" % (x, e)
    elif x < 0.01:
        e = 0
        while x < 1:
            x *= 10
            e += 1
        if abs(x - round(x)) < 1e-1:
            return "%de-%d" % (int(round(x)), e)
        else:
            return "%.1fe-%d" % (x, e)
    else:
        return x_str
        
def prettify_param(p):
    if p.startswith("logU"):
        i = p.find("e")
        j = p.find("d")
        return "($\\varepsilon =$ %s)" % format_param(p[i+1:j])
    elif p.startswith("ss"):
        i = p.find("use_new")
        if i == -1:
            i = len(p)
        return "($k =$ %s)" % format_param(p[2:i])
    elif p.startswith("e"):
        return "($\\varepsilon =$ %s)" % format_param(p[1:])
    elif p.startswith("l"):
        return "($\ell =$ %s)" % format_param(p[1:])
    return p
