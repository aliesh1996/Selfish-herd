import random 
import numpy as np
import math
import matplotlib.pyplot as plt



# the function for assigning random coordinates to each prey
def coordination(prey, start, stop):
    coords_x = [random.randrange(start,stop) for _ in range(prey)]
    coords_y = [random.randrange(start,stop) for _ in range(prey)]
    return coords_x,coords_y


# the function for random attacks
def random_attack(predator, prey):
    attacklist = np.arange(predator)
    for i in range(predator):
        attacklist[i]=random.randint(0,prey)
    return attacklist


# the function for outside attacks
def outside_attack(predator, prey):
    attacklist = np.arange(predator)
    for i in range(predator):
        attacklist[i]=random.randint(0,prey)
    return attacklist
 


# the function for random walk attacks
def randomwalk_attack(predator, prey, co_prey, co_predator, pre_attack):
    attacklist = np.arange(predator)
    for i in range(predator):
        while 1:
            firstchance = random.randint(0,prey)
            if math.dist(co_prey[firstchance], pre_attack[i])<50:
                attacklist[i]=firstchance
                break
    return attacklist


# the function for clustering the swarms
def cluster(centers, co_prey, radius):
    cluster_dedication = np.arange(len(co_prey[0]))
    for i in range(len(co_prey[0])):
        distance = radius
        cno = len(centers[0])+1
        for j in range(len(centers[0])):
            if math.dist((co_prey[0][i],co_prey[1][i]), (centers[0][j],centers[1][j]))<distance:
                distance=math.dist((co_prey[0][i],co_prey[1][i]), (centers[0][j],centers[1][j]))
                cno=j
        cluster_dedication[i]=cno
    return cluster_dedication




if __name__=="__main__":

    predator = 50
    prey = 250
    centerno = 4

    co_prey = coordination(prey, 50, 200)
    co_predator = coordination(25, 0, 50)
    co_predator[0].extend(coordination(25, 200, 300)[0])
    co_predator[1].extend(coordination(25, 200, 300)[1])
    centers = coordination(centerno, 50, 200)
    clusters = cluster(centers, co_prey, 100)

    for i in range(len(clusters)):

        if clusters[i]==0:
            plt.scatter(co_prey[0][i],co_prey[1][i], c='b')
        elif clusters[i]==1:
            plt.scatter(co_prey[0][i],co_prey[1][i], c='k')
        elif clusters[i]==2:
            plt.scatter(co_prey[0][i],co_prey[1][i], c='y')
        elif clusters[i]==3:
            plt.scatter(co_prey[0][i],co_prey[1][i], c='g')
        else:
            plt.scatter(co_prey[0][i],co_prey[1][i], c='m')


        

    # plt.scatter(co_prey[0],co_prey[1])
    plt.scatter(co_predator[0],co_predator[1], c='r')
    plt.show()

    

    

    









