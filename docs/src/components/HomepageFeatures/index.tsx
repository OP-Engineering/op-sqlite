import type { ReactNode } from 'react';
import clsx from 'clsx';
import Heading from '@theme/Heading';
import styles from './styles.module.css';

type FeatureItem = {
  title: string;
  description: ReactNode;
};

const FeatureList: FeatureItem[] = [
  {
    title: 'Fastest',
    description: (
      <>
        it's the fastest sqlite library for React Native. Binds directly to C++
      </>
    ),
  },
  {
    title: 'Multiple Backends',
    description: (
      <>
        Run against vanilla sqlite, or sqlcipher, or libsql. The choice is
        yours.
      </>
    ),
  },
  {
    title: 'Batteries Included',
    description: (
      <>
        Many plugins and extensions already come bundled. Just flip the switch
        on.
      </>
    ),
  },
];

function Feature({ title, description }: FeatureItem) {
  return (
    <div className={clsx('col col--4')}>
      <div className="text--center padding-horiz--md">
        <Heading as="h3">{title}</Heading>
        <p>{description}</p>
      </div>
    </div>
  );
}

export default function HomepageFeatures(): ReactNode {
  return (
    <section className={styles.features}>
      <div className="container">
        <div className="row">
          {FeatureList.map((props, idx) => (
            <Feature key={idx} {...props} />
          ))}
        </div>
      </div>
    </section>
  );
}
