import cv2
import dlib
import numpy as np


detector = dlib.get_frontal_face_detector()
predictor = dlib.shape_predictor("shape_predictor_68_face_landmarks.dat")

def getLandmarks(image):
    
    grayImage = cv2.cvtColor(image, cv2.COLOR_BGR2GRAY)
    landmarksPoint = np.zeros([20,2], dtype = np.int32)
    
    rects = detector(grayImage)
    
    for rect in rects:
        landmarks = predictor(grayImage, rect)
        
        for i in range(48, 68):
            x = landmarks.part(i).x
            y = landmarks.part(i).y
            
            landmarksPoint[i - 48] = (x,y)
            
    return landmarksPoint
        
    
    '''
    rects = detector(image, 1)
    return np.array([[p.x, p.y] for p in predictor(image, rects[0]).parts()])
    '''
    

def transformation(points1, points2):
    points1 = points1.astype(np.float64)
    points2 = points2.astype(np.float64)
    
    c1 = np.mean(points1, axis = 0)
    c2 = np.mean(points2, axis = 0)
    
    points1 -= c1
    points2 -= c2
    
    s1 = np.std(points1)
    s2 = np.std(points2)
    
    points1 /= s1
    points2 /= s2
    
    U, S, Vt = np.linalg.svd(points1.T @ points2)
    
    R = (U @ Vt).T
    
    return  np.hstack([(s2/s1) * R,
                       (c2.T - (s2 / s1) * R @ c1.T)[:, None]])

 

def createMask(points, shape, faceScale):
    
    maskIm = np.zeros(shape, dtype = np.float64)
    
    cv2.fillConvexPoly(maskIm, cv2.convexHull(smileLand), color = (1, 1, 1))
    
    featherAmount = int(0.2 * faceScale * 0.5 ) * 2 + 1
    kernelSize = (featherAmount, featherAmount)
    
    maskIm = (cv2.GaussianBlur(maskIm, kernelSize,0) > 0) * 1.0
    maskIm = cv2.GaussianBlur(maskIm, kernelSize, 0)
    
    return maskIm

def correctColours(warpIm, image, faceScale):
    blurAmount = int(3 * 1 * faceScale) * 2 + 1
    kernelSize = (blurAmount, blurAmount)
    
    faceBlur = cv2.GaussianBlur(warpIm, kernelSize, 0)
    bodyBlur = cv2.GaussianBlur(image, kernelSize, 0)
    
    return np.clip(0 + bodyBlur + warpIm - faceBlur, 0, 255)

    

smile = cv2.imread("normalFace1.jpg") # WITHOUT SMILE
normal = cv2.imread("smile.jpg")      # WITH SMILE 

normalLand = getLandmarks(normal)
smileLand = getLandmarks(smile)

M = transformation(points1 = normalLand,
                   points2 = smileLand)

warpedIm = cv2.warpAffine(normal,
                          M,
                          (smile.shape[1], smile.shape[0]))


fScale = np.std(smileLand)

'''
colours = correctColours(warpIm = warpedIm,
                         image = smile,
                         faceScale = fScale)
'''


mask = createMask(points = smileLand,
                  shape = smile.shape,
                  faceScale = fScale)

combined = (warpedIm * mask + smile * (1 - mask))

#cv2.imwrite('comibe.jpg', combined)

cv2.imshow("img4", combined)

cv2.waitKey(0)
cv2.destroyAllWindows()

