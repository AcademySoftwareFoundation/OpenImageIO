def crypt():
    import OpenImageIO as o
    import array
    import random
    import pickle

    path = raw_input("Enter OIIO plugin path: ")
    in_file = raw_input("Enter the name of the file for encryption: ")
    out_file = raw_input("Enter the name of the resulting file: ")
    key_path = raw_input("Enter the name of the file in which to store the key: ")

    # open the input file
    spec = o.ImageSpec()
    pic = o.ImageInput.create(in_file, path)
    pic.open(in_file, spec)
    desc = spec.format

    # Create a couple of arrays where we'll store the data.
    # They all need to be the same size, and we'll fill them with dummy data for now.
    # We'll read the original pixel values in arr
    arr = array.array("B", "\0" * spec.image_bytes())
    # the values we'll write to the encrypted file
    new_values = arr[:]
    length = range(len(new_values))
    print "Working, please wait..."
    pic.read_image(desc, arr)

    # save the state of the random number generator so we can use it 
    # to decode the image
    state = random.getstate()

    # generate random values, add them to the original values.
    # Do % 256 so nothing overflows
    for i in length:
        rand_val = random.randint(0, 255)
        new_values[i] = (arr[i] + rand_val) % 256
        
    # write new values to the output file, close everything
    out = o.ImageOutput.create(out_file, path)
    out.open(out_file, spec, False)
    out.write_image(desc, new_values)
    out.close()
    # save the state of the RNG - that's the key for decryption
    f = open(key_path, "w")
    pickle.dump(state, f)
    f.close()
    return True


def decrypt():
    import OpenImageIO as o
    import array
    import pickle
    import random
    path = raw_input("Enter OIIO plugin path: ")
    key_path = raw_input("Name of the file with the key: ")
    in_file = raw_input("Name of the encrypted file: ")

    # Open the input files, read the RNG state and store it in "key"
    f = open(key_path, "r")
    key = pickle.load(f)
    
    spec_cr = o.ImageSpec()
    pic_cr = o.ImageInput.create(in_file, path)
    pic_cr.open(in_file, spec_cr)
    desc_cr = spec_cr.format

    # The encrypted pixel values will be stored here.
    # The decoding will be done inplace, so that will also be
    # the output buffer once the decryption is done.
    arr_cr = array.array("B", "\0" * spec_cr.image_bytes())
    length = range(len(arr_cr))
    print "Working, please wait..."

    # Let's read the encrypted image
    pic_cr.read_image(desc_cr, arr_cr)
    
    # Set the state of the RNG to match the state of the RNG which coded 
    # the image. After this, we can generate the same random sequence 
    # used to code the image.
    random.setstate(key)
    
    # Decryption!
    for i in length:
        rand_val = random.randint(0, 255)
        # This is the inverse of the encryption.
        restored_pixel = arr_cr[i] - rand_val
        if restored_pixel < 0:
            arr_dec[i] = 256 + restored_pixel
        else:
            arr_dec[i] = restored_pixel

    print "Decryption completed!"
    image = raw_input("Enter the name under which to store the result: ")
    print "Working, please wait..."    
    out_dec = o.ImageOutput.create(image, path)
    out_dec.open(image, spec_cr, False)
    out_dec.write_image(desc_cr, arr_dec)
    out_dec.close()
    return True





